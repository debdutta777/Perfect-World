# Front-end redesign phase 1 — 11 October 2026

- **Implemented in source, uncommitted:** cancellable delayed focus with hidden/disabled subtree rejection; Cancel-first save-slot selection and explicit replacement confirmation; pause departure confirmations; in-session save recovery; responsive creator shell with a persistent stepper/footer and one scrolling body; shared palette, typography, controls and dialog shadow; title scrim, save-aware primary action, Credits and bounded camera drift.
- Hind Siliguri uses one style-owned composite family, matching static weights and full shaping. Filename-backed faces use `EFontLoadingPolicy::LazyLoad`; inspected UE 5.8's font cache calls `FFileHelper::LoadFileToArray`, compatible with UFS staging. `DefaultGame.ini` stages all of `UIResources`, including the existing OFL licence.
- **Static validation only:** reviewed local UE 5.8 headers for font constructors/loading, gradient/box painting, animation, render transforms, opacity setters, scroll navigation and text shaping. Modified C++ files include their own header first, stay below 500 lines and pass delimiter checks. UE 5.8 exposes render opacity as a construction argument, so animation updates it through `SetRenderOpacity` in Tick; translation remains a render attribute and layout is not rebuilt.
- **Not verified:** Unreal compilation, PIE, cooked UFS loading, Bengali IME/rendering, or mouse/keyboard/controller interaction at 1280×720, 1366×768, 1600×900 and 1920×1080. No Unreal build was run, as requested. Portable core, Data, Tests, DefaultEngine.ini and DefaultInput.ini were not edited.
- **Next:** phase 2 should redesign the trait rows/inspector and household/stream previews, obtaining numeric effects and any mutation/undo support from the portable core rather than duplicating rules in Slate. Run the phase-1 Unreal and packaged interaction checks before visual acceptance.

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

## Checkpoint — M2-06 schedules and minimal off-screen progress, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:** new `Public/Schedules.h`, `Private/{Schedules.cpp, SchedulePlan.cpp, ScheduleText.cpp, ScheduleInternal.h}` and `Tests/{SchedulesTests.cpp, SchedulesProgressTests.cpp, SchedulesTestSupport.h}`; `CMakeLists.txt`.

**Rules:**
- **Pure resolution:** `ResolveSchedule(person, travel, t)` is pure, following "Nobody walks around offscreen" (`SYSTEMS.md` §1) and the catalogue's `WhereIs`. It looks only 8 days either side of the instant.
- **Routine and exceptions:** weekly routine entries plus dated exceptions, decided by priority. On a tie an exception beats routine, then the later start, then the smaller ID.
- **Travel:** a person lingers in the previous zone, then leaves to arrive exactly on time. When a gap is too short for the trip, the lower-priority stay is shortened and both are marked `Squeezed`.
- **Off-screen advance:** steps only at event boundaries and tallies sessions and minutes per activity. Unseen classmates therefore spend the same calendar, and a 16-hour jump equals boundary stepping equals minute stepping.
- **Formats:** three strict text formats with hashes, for schedules, travel tables and person state.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 49/49, `Schedules tests passed (275421 checks)`. The tests cover:
- 10,000 random schedules
- additivity and determinism
- event emitted exactly when the resolved identity changes
- every truncation and every single-byte change refused

**Open questions:**
1. The `Data/Schedules` JSON shape and its validator are not decided.
2. NPCs are never late, because squeezing shortens stays instead. M2-07 may want NPC lateness.
3. Not yet profiled against the 2 ms boundary budget.

## Checkpoint — M2-11 opportunity receipts, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:** new `Public/OpportunityReceipts.h`, `Private/{OpportunityReceipts.cpp, OpportunityReceiptsState.cpp, OpportunityReceiptsText.cpp, OpportunityReceiptsInternal.h}` and `Tests/{OpportunityReceiptsTests.cpp, OpportunityReceiptsTextTests.cpp, OpportunityReceiptsTestSupport.h}`; `CMakeLists.txt`.

**Rules** (from `SYSTEMS_CATALOGUE.md` "Opportunity receipts" and the `GAME_PLAN.md` glossary):
- **What gets displaced:** a commitment records an alternative only if all of these hold: it was known at the decision, it was available then, its window overlaps the commitment's (windows are half-open), and it is a different activity. Later knowledge never rewrites a receipt.
- **Ranking:** alternatives are ranked by forecast learning, and at most 8 are stored per commitment. The rest are summarised, so totals stay exact.
- **Per day:** a commitment belongs to the day it starts on. A day keeps at most 24 entries, with an overflow running sum beyond that.
- **Append-only:** stored entries never change.
- **Folding:** weeks older than 120 days fold into weekly totals.
- **Query:** `ReceiptDayQuery` answers "what did today cost me".
- **Save format:** `pw.receipts/1` text, which refuses truncation, non-canonical input and invalid states.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 50/50, `OpportunityReceiptsTests passed (4507301 checks)`. The tests cover:
- exact displacement sets
- order independence
- the per-commitment and per-day caps
- folding that preserves totals
- 10,000 random sequences against an independent oracle, with save and load midway

**Open questions:**
1. Weeks open on Monday; the owner may want the school week instead.
2. The catalogue's other receipt line kinds (`covered`, `earned`, `spent`, `decided_by_order`) wait for integration.
3. `KnownAt` must be stamped from real discovery events when this is wired in.

## Checkpoint — Unreal 5.8.3 reinstalled after the second wipe; Rung 2 and engine tests green, 2026-10-10

**What happened:** the first Rung 2 on the fresh engine failed in `AssetGameplay.cpp`: a callback parameter named `check` was macro-expanded by Unreal's `check()` assertion (`binary '!': no operator found`). Renamed it to `checkObject` and recorded the trap in `DEVELOPMENT_WORKFLOW.md` §4.

**Exact checks:**
- `python Scripts/build_unreal.py --engine-root "D:\Epic Games\UE_5.8"` (D: clone, MSVC 14.50.35739): **Succeeded**, no warnings. This compiles every core module merged since 2026-10-05 inside Unreal.
- Editor opened, Unreal MCP `RunTestsByFilter "StartsWith:PerfectWorld.Core"`: **4/4 Success**, including `ContentLoadsInEngine` at 86 files and 122 records.
- `python Scripts/build_core.py --build-dir D:/pwm`: PASS, 50/50.

## Checkpoint — M2-08 "Model a system" structure and its classroom family, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:**
- New: `Public/{StructureModel.h, StructureModelCatalogue.h}`, `Private/{StructureModel.cpp, StructureModelRun.cpp, StructureModelText.cpp, ContentProblemSchemas.cpp}` and `Tests/{StructureModelTests.cpp, StructureModelFlowTests.cpp, StructureModelContentTests.cpp, StructureModelTestSupport.h}`.
- Registration lines in `ContentSchemas.{h,cpp}`.
- Data under `Data/Content/`: `structure` 1, `problem_family` 1, `problem_case` 8. The registry requires `<type>/<id>.json`, so this is not `Data/Problems`.
- Seven bad fixtures.
- `CMakeLists.txt`.
- The pinned counts in `PerfectWorldCoreParityTests.cpp`, now 96 files and 132 records.

**Rules** (from `SCHOOL_AND_PROGRESSION.md`: the water-tank example, "Time economy", "what failure means", "established routine"):
- **Beats:** three per exposure: build the model, resolve the discrepancy, present a claim. Each runs in 5-minute units with every price quoted before commit, and an exposure can never strand.
- **Failure:** a wrong model or an unsupported conclusion grades Unsound; wasted time or an unclear explanation grades Partial.
- **Case selection:** deterministic, never repeating the last three cases shown.
- **Routine abbreviation:** for a secure family, resolves an exposure off-screen at the normal learning value, with nothing observed.
- **Cases:** eight Class XI stock-and-flow cases (the water tank, the overhead tank, the stationery counter, a cash daybook, a monsoon canal, a bus queue, a cricket required rate, roof rainwater). All have distinct (unknown, role, scarce resource) triples, and every value is used at least once.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 58/58. `StructureModelTests passed (131015 checks)`; `pwcontent validate Data/Content`: `PASS: 96 files, 132 records`.

**Open questions:**
1. The unit budgets rarely bind.
2. An honest hedge (accepting the readings and giving a bounded range) grades the same as investigating.
3. Mastery counts cases met, not cases passed.
4. The new parity counts need Rung 2 and an editor run.

## Checkpoint — M2-07 Nabapara Vidyalaya bell schedule, attendance and lateness, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here. Its registration lines in `ContentSchemas.{h,cpp}` were merged by hand next to M2-08's.

**Files changed:**
- New: `Public/Attendance.h`, `Private/{AttendanceBell.cpp, Attendance.cpp, AttendanceText.cpp, AttendanceInternal.h, ContentScheduleSchemas.cpp}` and `Tests/{AttendanceTests,AttendanceRoutineTests,AttendanceRecordTests,AttendanceTextTests}.cpp` plus `AttendanceTestSupport.h`.
- Data: three `Data/Content/bell_schedule/bell_schedule.nabapara_xi_*.json`.
- Two fixture directories with seven rules.
- `CMakeLists.txt`.
- Pinned parity counts, now 99 files and 135 records.

**Timetable:** one shared frame. The gate opens at 09:30, assembly runs 10:00–10:15, then six 45-minute periods with tiffin from 12:50 to 13:20.
- **Science:** mathematics, physics, chemistry, english, physics lab, bengali.
- **Commerce:** accountancy, business studies, economics, english, mathematics, bengali.
- **Humanities:** history, geography, political science, english, sociology, bengali.

The subjects are WBCHSE-style assumptions flagged for a consultant check. A bell schedule becomes M2-06 routine entries, so classmates and the player share one calendar.

**Lateness:**
- At or before the 10:00 bell: on time.
- 1 to 5 minutes after: late, one late mark.
- 6 minutes or more after: very late. One late mark, assembly missed, and any period more than 5 minutes started is missed.
- No period attended: absent.
- A teacher warns at every third late mark, and the guardian is told at the sixth.
- Below 85% period attendance is Warned; below 75% is AtRisk (no board examination).
- NPCs are never late, because M2-06 squeezes stays instead.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 66/66. `AttendanceTests passed (167422 checks)`; `pwcontent validate Data/Content`: `PASS: 99 files, 135 records`.

**Open questions:**
1. Holidays and Saturday school are not modelled; every Monday to Friday is a school day.
2. The `nhss.*` zone names are placeholders until M2-13.
3. The new parity counts need Rung 2 and an editor run.

## Checkpoint — engine tests at 99/135, the derived-data cache moved to D:, 2026-10-10

**Engine side (D: clone at `b441347`, which includes M2-07 and M2-08):**
- **First Rung 2 attempt:** a `cl.exe` compiling `PerfectWorldCoreParityTests.cpp` hung in a kernel wait with no CPU and no disk load. I killed it and reran: **Succeeded** in 149 s, no warnings.
- **Unreal MCP `RunTestsByFilter "StartsWith:PerfectWorld.Core"`:** **4/4 Success**, with `ContentLoadsInEngine` at 99 files and 135 records.
- **Packaging attempt:** the cook stalled for over 10 minutes at startup while bringing up the Zen derived-data cache service, whose data folder was on C: (about 10 GB free). The run was stopped.
- **Cache move:** the machine-local file `%LOCALAPPDATA%\Unreal Engine\Engine\Config\UserEngine.ini` now sets `[Zen.AutoLaunch] DataPath=D:/UnrealDDC/Zen/Data`. This is not project config. The editor's zenserver now runs with `--data-dir D:\UnrealDDC\Zen\Data`.

## Checkpoint — M2-09 explain, investigate and coordinate structures, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:**
- New: `Public/{StructureCommon,StructureExplain,StructureInvestigate,StructureCoordinate}.h`, `Private/{StructureCommon.cpp, StructureInternal.h}`, three `Structure<X>{,Run,Text}.cpp` per structure, `Private/{ContentStructureSchemas.cpp, ContentStructureCases.cpp, ContentStructureChecks.h}`, and tests under `Tests/Structure{Others,Explain,Investigate,Coordinate,Distinct}*`.
- Small registration edits in `ContentSchemas.{h,cpp}` and `ContentProblemSchemas.cpp`.
- Data: 3 `structure`, 3 family and 12 case records under `Data/Content`.
- Five fixture directories with 22 rules.
- `CMakeLists.txt`.
- Pinned parity counts, now 117 files and 153 records.

**Structures** (from `SCHOOL_AND_PROGRESSION.md`, and `ALTERNATE_WORLD.md` for investigate):
- **Explain:** claim, evidence, limitation. Every case has at least two defensible claims, so there is no hidden right answer. Failures are indefensible, unsupported, overreach and padding.
- **Investigate:** an adaptive loop of tests with different costs, then a conclusion. Failures are wrong cause, unsupported (a lucky guess is still Unsound), redundant test and abandoned search; an honest "not isolated" report is Sound.
- **Coordinate:** assign, exchange, settle, submit, around one hidden constraint revealed only by a classmate's facts. Failures are overloaded, mismatched, uninformed, unsettled, an unchecked seam and wasted effort.
- **Shared conventions:** all three share M2-08's time economy, quote-before-commit, case selection, mastery and routine abbreviation.
- **Distinctness:** the four structures share no beat, decision or failure name, and the standard play of one case each gives four different decision sequences.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 89/89. `StructureOthersTests passed (390535 checks)`; M2-08's 131,015 checks are unchanged; `pwcontent validate Data/Content`: `PASS: 117 files, 153 records`.

**Open questions:**
1. The new families use their own record types (`explain_family` and so on) rather than `problem_family`/`problem_case`, which would have required changing M2-08's loader.
2. The 117/153 parity counts need Rung 2 and an editor run.

**Engine verification of M2-09 (D: clone at `d0cdc36` plus the fix below):** Rung 2 printed `error: Expected StructureCommon.h to be first header included.` for `StructureCommon.cpp`, though the build still reported Succeeded. After the source included its own header first, Rung 1 passed 89/89 and Rung 2 succeeded with no error lines. Unreal MCP `RunTestsByFilter "StartsWith:PerfectWorld.Core"`: **4/4 Success**, with `ContentLoadsInEngine` at 117 files and 153 records.

## Checkpoint — Rung 4 again on the reinstalled engine, 2026-10-10

**Exact check:** `python Scripts/package_windows.py --engine-root "D:\Epic Games\UE_5.8" --output D:\PWBuilds` (D: clone at `09f9f2b`, editor closed, derived-data cache on D:): **exit 0**, `BUILD SUCCESSFUL`. Then `D:\PWBuilds\Windows\PerfectWorld.exe -windowed`:
- The window appeared in 10 s.
- The log shows `Game class is 'PerfectWorldBlockTestGameMode'` and no `Error:` or ensure.
- Closing the window gave `RequestExit(0)` and `LogExit: Exiting.` after about 16 s of shutdown.
- No crash reporter appeared.

The first attempt that session hung in the Zen cache start-up with the cache on C:; moving it to D: fixed it (see the earlier checkpoint). The owner's look-around check is still to do.

## Checkpoint — M2-12 day loop and recovery checkpoints, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:** new `Public/{DayLoop.h, Checkpoints.h}`, `Private/{DayLoop.cpp, DayLoopApply.cpp, DayLoopText.cpp, Checkpoints.cpp}` and `Tests/{DayLoopTests, DayLoopRefusalTests, DayLoopSaveTests, DayLoopRandomTests}.cpp` plus `DayLoopTestSupport.h`; `CMakeLists.txt`. No other module changed.

**Design:**
- **State:** `DayLoopState` composes the modules' own states: condition, activity runner, practice window, receipts and attendance. It adds the clock, a derived phase (plan, school, free block, evening, sleep), the plan, the current commitment, the last action and a dense command sequence.
- **Commands:** nine day commands through one `ApplyDayCommand`, which works on a copy and re-validates, so a refusal changes nothing.
  - Completing an activity routes learning through the practice window and records the receipt with the alternatives shown at commit.
  - Waking advances condition and the clock and closes attendance gaps.
- **Envelope:** `pw.dayloop/1` embeds each module's own text in length-prefixed sections, with one FNV-1a hash. The parse is strict, and re-serialising must reproduce the input byte for byte.
- **Checkpoints:** `checkpoint_1..3.txt`, rotated by wake count, which can never equal a `creation_slot_N.json` life slot.
- **Summary:** `DayLoopSlotSummary` gives the current commitment and the last action.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 90/90, `PerfectWorld day loop: 819044 checks passed`. The tests cover:
- A scripted Monday from 06:00 to Tuesday 06:00, 29 commands. The player arrives on time and attends 6 of 6 periods. Learning totals 6,250 milli-XP, matching the command log, and the condition matches an independent simulation.
- Saving and resuming at every command boundary of a day (30) and of a five-day week (146), each reaching the same final hash.
- Checkpoints that never touch life-slot files.
- About 85 refusals, each leaving the state unchanged.
- 1,000 random 70-command sequences that replay identically.

**Choices for review:**
1. The practice family is `family.<outcome>`, so all class periods share one window and the third period of a day earns only a tenth. The docs imagine separate subjects.
2. A morning plan (possibly empty) is required before time can be spent.
3. Activities are scene-mode: the clock jumps at completion.
4. **Finding:** at the campaign's starting energy of 80,000, an ordinary school day reaches the exhaustion-neglect threshold. The scripted day starts at 100,000 instead, and the Condition tuning probably needs a look.

**Integration still needed:**
- A `dayloop` section in the v6 world save, with a hash version bump.
- One shared clock.
- Routing the reducer's activity and time commands through the loop.
- The Unreal subsystem writing checkpoints.
- Reconciling `checkpoint_N.txt` with SYSTEMS §6's `.json` naming.

## Checkpoint — M3-05 Aptitude Projection, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:** new `Public/AptitudeProjection.h`, `Private/{AptitudeProjection.cpp, AptitudeProjectionProject.cpp, AptitudeProjectionText.cpp, AptitudeInternal.h}` and `Tests/{AptitudeProjectionTests.cpp, AptitudeProjectionTextTests.cpp, AptitudeTestSupport.h}`; `CMakeLists.txt`.

**Rules:**
- **Paths and evidence:** the ten paths from `PATHS_AND_PROGRESSION.md` "The ten paths", each with five evidence kinds ("facets"). Four facets come from the four learning structures; six come from practice, activities and attendance.
- **Bands:** each path gets one of Insufficient observation, Early evidence, Plausible or Strong, with a coverage level and an explicit list of the evidence kinds not yet observed. The printout says "unknown, not unsuitable".
- **No farmable number:** tallies stay private, and the tests check that no public text contains a digit.
- **Saturation:** repeats of the same source, facet and subject count 8, 4, 2, 1, then nothing, and only the best 4 are kept. One source kind can never establish a facet by itself.
- **Signature facet:** each path's first facet must be shown, so a pure study life does not read as Plausible for athletics.
- **Other guarantees:** adding evidence never lowers a band; the ledger is an order-independent set; a missed class is never recorded as weakness.
- **Save format:** `pw.aptitude/1` text, which refuses truncation.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 91/91, `AptitudeProjectionTests passed (782371 checks)`. The tests cover:
- each path's unobserved list on an empty ledger
- 10,000 identical repeats ending identical to 4 repeats
- structures alone never reaching Strong
- a study-heavy persona and a repair-heavy persona diverging as expected
- order independence under shuffling

**Open questions:**
1. Every number and the facet lists are chosen; the docs give the shape only.
2. A fourth band, "Early evidence", was added beside the catalogue's three, and its `ConfidenceMilli` was dropped.
3. The skill-to-facet mapping could move into data.
4. Not yet fed from the day loop's events.

## Checkpoint — M3-09 the 2042–43 calendar, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:**
- New: `Public/SchoolCalendar.h`, `Private/{SchoolCalendar.cpp, SchoolCalendarRules.cpp, ContentCalendarSchemas.cpp}` and `Tests/{SchoolCalendarTests.cpp, SchoolCalendarContentTests.cpp}`.
- Data: `Data/Content/calendar/calendar.nabapara_2042_43.json`.
- The `bad_calendar` fixtures (15 rules).
- Registration lines in `ContentSchemas.{h,cpp}`.
- `CMakeLists.txt`.
- Pinned parity counts, now 118 files and 154 records.

**Calendar:** the window runs 2 Jun 2042 to 27 Feb 2043.
- **Fixed dates:** Independence Day 15 Aug, Gandhi Jayanti 2 Oct, Christmas 25 Dec, Republic Day 26 Jan.
- **From `GAME_PLAN.md` §4:** Awakening on Fri 23 Jan 2043 and the examination on Mon 2 Feb 2043, ten days later.
- **Approximate, to be confirmed:** Durga Puja 20–24 Oct 2042, Kali Puja and Diwali 12 Nov 2042, Saraswati Puja 14 Feb 2043.
- **Assumptions:** three terms, a Puja vacation (18 Oct – 2 Nov), a winter break (24 Dec – 4 Jan), "the ten days" with no school, four assessment windows, and sports day on 5 Dec.
- **Totals:** 168 school days, 149 of them on or before Awakening.

The validator enforces real dates, ordered non-overlapping ranges, full coverage of the window by terms or breaks, the ten-day gap, and that lunisolar festival dates are marked approximate. The queries cover school day, holiday, term, break, assessment, the next and previous school day, and counts. All are checked against an independent oracle over every day and 3,000 random ranges.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 107/107, `PerfectWorldSchoolCalendarTests: 11841 checks passed`; `pwcontent validate Data/Content`: `PASS: 118 files, 154 records`.

**For the owner:**
1. Saraswati Puja (about 14 Feb 2043) falls after the examination, so the window runs on to 27 Feb, with school resuming 3 Feb. Confirm, or choose a window that ends at the examination.
2. Netaji Jayanti (23 Jan) is left out because Awakening Day must be a school day.
3. The terms, the Puja vacation length and the assessment dates need an education consultant.

**Next:** switch Attendance to the calendar so holidays are not counted as absences (the hook is described in `SchoolCalendar.h`), then run Rung 2 and the editor test for 118/154.

**Engine verification of M2-12, M3-05 and M3-09 (D: clone at `879c9df`):** the first Rung 2 stalled in the same `PerfectWorldCoreParityTests.cpp` compile as before. Ending the stray `vctip.exe` processes did not help; killing `cl.exe` and rerunning gave **Succeeded** (232 s), no warnings or error lines. The trap is now in `DEVELOPMENT_WORKFLOW.md` §4. Unreal MCP `RunTestsByFilter "StartsWith:PerfectWorld.Core"`: **4/4 Success**, with `ContentLoadsInEngine` at 118 files and 154 records.

## Checkpoint — M4-01 conduct ledger, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:** new `Public/ConductLedger.h`, `Private/{ConductLedger.cpp, ConductLedgerText.cpp}` and `Tests/ConductLedgerTests.cpp`; `CMakeLists.txt`.

**Rules:**
- **Dimensions:** the docs name only the dungeon's verbs and situations (`AWAKENING_AND_EXAM.md`, catalogue "Tutorial dungeon"). The module therefore uses five chosen dimensions (care, steadfastness, restraint, resourcefulness, attentiveness) and six actions (observe, assist, evade, improvise, withdraw, hold).
- **Evidence:** keyed on the action alone, so every action weighs the same and withdrawing equals a rescue. Evidence is never negative.
- **Bounds:** stakes run 1 to 3, set per test by the author. Each test is capped at 10 per dimension and 14 in total. The ledger caps each dimension at 24.
- **Bands:** NotObserved, Slight, Clear and Strong. No single test can make a dimension Strong.
- **Once per test:** the first resolution of each test stands, and replays are refused.
- **Fairness:** tokens naming religion, caste, language, stream, gender, family or background are refused.
- **Summary:** for M4-02, bands with bounded milli values and named evidence, with no overall verdict.
- **Save format:** `pw_conduct 1` text, which refuses truncation and corruption.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 108/108, `PerfectWorldConductTests: 330079 checks passed`.

**For the owner:** confirm the five dimensions and six actions before they reach saves. M4-02 must map them to the Tattvas, and decide how a student absent from the dungeon is treated without penalty.

## Checkpoint — attendance follows the school calendar, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:**
- `Attendance.{h,cpp}`: `AttendanceIsSchoolDay(const SchoolCalendar*, day)`, plus an optional trailing calendar argument on `RecordAttendanceArrival`, `RecordAttendanceAbsence` and `CloseAttendanceBefore`, and the code `attendance.not_school_day`.
- `DayLoop.h` and `DayLoopApply.cpp`: `DayLoopCatalogues::Calendar`, passed through at school arrival and at wake.
- New `Tests/{AttendanceCalendarTests.cpp, DayLoopCalendarTests.cpp}`, with small hooks in the existing test mains.
- `CMakeLists.txt`.

**Behaviour:**
- With a calendar: holidays, breaks, weekends, gaps between terms and days outside the year are never absences, and recording on one is refused with the holiday named.
- Without a calendar: nothing changes. The save text is byte-identical either way.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 108/108. `AttendanceTests passed (168902 checks)`, `PerfectWorld day loop: 819112 checks passed`. The tests show Independence Day and the Puja vacation are not absences, a missed ordinary day still is, and the day loop refuses arrival at school on 15 Aug.

**Limitation:** `ValidateAttendanceRecord` still assumes Monday to Friday, so a future calendar with Saturday classes would need the validator to read the calendar.

## Checkpoint — M3-07 assessments, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:**
- New: `Public/Assessment.h`, `Private/{Assessment.cpp, AssessmentRun.cpp, AssessmentText.cpp, ContentAssessmentSchemas.cpp}` and `Tests/Assessment{,Marking,Save,Content}Tests.cpp` plus `AssessmentTestSupport.h`.
- Registration lines in `ContentSchemas.{h,cpp}`.
- Data: `Data/Content/assessment_paper/assessment_paper.xi_half_yearly_mixed.json`.
- Eight bad fixtures.
- `CMakeLists.txt`.
- Pinned parity counts, now 119 files and 155 records.

**Paper:** 90 minutes in 18 units, set in the calendar's half-yearly window. Five groups of 20 marks draw on all four structures: two model cases, one explain, one investigate, one coordinate.

**Play:**
- **Survey:** costs 1 unit, is optional, and shows familiarity plus a price range that always contains the true price.
- **Allocation:** units are promised to groups and cap what each may spend.
- **Answers:** Work costs 2 units if the method is practised and 4 if not; Confirm adds 1; Guess costs 1; Hedge is free. Every price is quoted before commit.
- **Economy:** a prepared student fits in 16 of 18 units; an unprepared one must triage.

**Marking:** each structure's own failure bits map to the four kinds from `SCHOOL_AND_PROGRESSION.md` "What varies on the fortieth class": wrong model, unsupported conclusion, wasted time and unclear explanation. Sound earns full marks, Partial 750‰, Unsound 250‰, and an unattempted group 0.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 117/117. `AssessmentTests passed (131210 checks)`; `pwcontent validate Data/Content`: `PASS: 119 files, 155 records`.

**Open questions:**
1. The doc's step 6 names four different failure kinds; the "fortieth class" set was used.
2. Questions use an abbreviated resolution rather than the full 45-minute structure runners, to keep the documented paper economy.
3. The 119/155 parity counts need Rung 2 and an editor run.

**Engine verification of M4-01, the attendance calendar change and M3-07 (D: clone at `3abffe8`):** Rung 2 **Succeeded** (28 actions, no warnings or error lines). Unreal MCP `RunTestsByFilter "StartsWith:PerfectWorld.Core"`: **4/4 Success**, with `ContentLoadsInEngine` at 119 files and 155 records.

## Checkpoint — M4-02 the Unified Awakening, and M1-34 the starting profile, 2026-10-11

Built by a Sonnet 5.5 subagent. The agent was cut off twice by session ends before writing its report, so I built and tested its worktree myself, reviewed it, and merged it here.

**Files changed:**
- New: `Public/Awakening.h`, `Private/{AwakeningSignals.cpp, AwakeningResolve.cpp, AwakeningRecipes.cpp, AwakeningProfile.cpp, AwakeningInternal.h, ContentAbilitySchemas.cpp}` and `Tests/{AwakeningTests.cpp, AwakeningPersonaTests.cpp, AwakeningProfileTests.cpp, AwakeningPersonas.h, AwakeningTestSupport.h}`.
- Registration lines in `ContentSchemas.{h,cpp}`.
- Data: 15 `Data/Content/ability_recipe/*.json`, three per Tattva (prithvi, jala, agni, vayu, akasha).
- Bad fixtures.
- `CMakeLists.txt`.
- Pinned parity counts, now 134 files and 170 records.

**Design** (from `AWAKENING_AND_EXAM.md`, `AWAKENED_GRADES.md` §1 and §4, `PATHS_AND_PROGRESSION.md` and the catalogue; every number is a named constant, chosen where the docs are silent):
- **Inputs:** a pure function of the conduct summary, latent stats, the aptitude projection and the world seed. Inputs are reduced to thirteen readings: eight from the sixteen years of life and five from conduct. Missing evidence reads as zero, never as weakness.
- **Tattva distribution:** five values summing to 100,000, mixing life and conduct 60:40. Every Tattva keeps a floor.
- **Capability:** six ratings, each a life term plus a share of a fixed conduct budget. Conduct decides where the dungeon's contribution goes, never how much, so withdrawing, rescuing and being absent all receive the same total. Ratings are at most 130, below grade F's ceiling of 149.
- **Absence:** an absent student is not penalised. The conduct share is dropped and the budget split evenly.
- **Ability:** exactly one ability, from the qualifying evidence recipe with the best score in the leading Tattvas, or else the leading Tattva's fallback.
- **Fairness:** stream, household, appearance, pronouns, languages and names are not inputs.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 129/129, the awakening tests included; `pwcontent validate Data/Content`: `PASS: 134 files, 170 records`. The worktree build had already passed 129/129 on its own.

**Open questions:** the signal weights, the 60:40 mix, the floors, budgets and recipes are all tuning, not documented values. The conduct-to-Tattva mapping needs the owner's review together with the conduct dimensions from M4-01.

## Checkpoint — M3-06 threads, 2026-10-11

Built by a Sonnet 5.5 subagent. The agent was cut off by session ends before writing its report, so I built and tested its worktree myself (122/122 there), reviewed it, and merged it here. The registration lines, CMake entries and pinned counts were merged by hand next to M3-07's and M4-02's.

**Files changed:**
- New: `Public/Threads.h`, `Private/{Threads.cpp, ThreadsRun.cpp, ThreadsText.cpp, ThreadsInternal.h, ThreadTemplates.cpp, ThreadTemplateRules.cpp, ContentThreadSchemas.cpp}` and `Tests/Threads{,Content,Run,Equivalence,Save}Tests.cpp` plus `ThreadsTestSupport.h`.
- Registration lines in `ContentSchemas.{h,cpp}`.
- Data: six `Data/Content/thread_template/*.json` (borrowed notes, a canal-side stall, an evening rota, a hidden pickup, refused invitations, a sports-day rival).
- The `bad_thread` fixtures (13 rules).
- `CMakeLists.txt`.
- Pinned parity counts, now 140 files and 176 records.

**Design** (from `SYSTEMS_CATALOGUE.md` "Threads" and `ALTERNATE_WORLD.md` §8):
- **Templates:** each names 2 to 4 roles (never people) and 3 to 6 stages forming an escalation clock (rising intensity, dwell in days, a default resolution). Triggers are plain data with one effect each, and expressions are what the player can observe.
- **Validator:** the first stage must be quiet; the default resolution must not be a repair; every resolution must be reachable; the player must have at least one way to change the course; every role must be used; calendar references must exist.
- **Fairness:** no template may depend on religion, caste, language, stream, gender or family background.
- **Instances:** each binds roles to registered identities and advances at event boundaries, replay-stable. The text save refuses truncation.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 143/143, `PerfectWorldThreadTests` `PASS: 176142 checks`; `pwcontent validate Data/Content`: `PASS: 140 files, 176 records`.

**Remaining:** not in the world state, the day loop or the save; the 140/176 parity counts need Rung 2 and an editor run.

## Checkpoint — M3-01 time contract, portable half, 2026-10-11

Built by a Sonnet 5.5 subagent. The agent hit the usage limit during its final build, so I built and tested the worktree myself (144/144 there), reviewed it, and merged it here.

**Files changed:** new `Public/TimeContract.h`, `Private/{TimeContractRules.cpp, TimeContractResolve.cpp, TimeContractText.cpp, TimeContractContent.cpp, TimeContractEquivalence.cpp}` and `Tests/TimeContract{,Resolve,Content,Equivalence}Tests.cpp` plus `TimeContractTestSupport.h`; `CMakeLists.txt`. No existing module changed.

**Rules** (`GAME_PLAN.md` §5, `SYSTEMS.md` §2.5, catalogue "Calendar and time granularity"):
- **Modes:** six clock modes plus compressed time.
  - free_roam runs the world clock at 1 game minute per real second.
  - conversation, scene, ui_paused and accessibility_pause pause the world.
  - mission runs its own clock and charges the world a capped amount at exit.
  - compressed jumps event by event.
- **Declarations:** each declaration is `clock{mode, interrupt, resolve}` plus a cost and a number of beats. Every activity in `Data/Content/activity` validates against the rules.
- **Interrupt policies:** none, at_beat and immediate; immediate charges pro rata.
- **Resolution boundaries:** on_commit, on_complete and per_beat.
- **Equivalence:** a helper shows a plan resolved in one jump or step by step gives the same results for the documented quantities. This is the base M3-02 builds on.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 144/144, `PerfectWorld time contract: 205778 checks passed`.

**Remaining:** the Unreal (PIE) half: the free-roam ticker and the pause overlays that read this state.

## Checkpoint — the player's day joins the world state, the world hash (version 4) and the world save (schema v7), 2026-10-11

Built by a Sonnet 5.5 subagent. The agent hit the usage limit just after pinning the goldens, so I built and tested the worktree myself (143/143 there), reviewed it, and merged it here next to the time contract.

**Files changed:**
- `WorldState.h`: `DayLoopState DayLoop` in the world, `MakeWorldState` starting a fresh day, and `IsWorldDayInStep`.
- New `Private/WorldDay.cpp`.
- `WorldHash.{h,cpp}`: a `dayloop` section and `WorldHashVersion` 4.
- `WorldSave.{h,cpp}`, `WorldSaveReader.cpp` and `WorldSaveInternal.h`: schema v7 with a `dayloop` key carrying the `pw.dayloop/1` envelope.
- `WorldReducer.{h,cpp}`: a time advance moves both clocks.
- Tests: `WorldHashReference.h` and `WorldHashTests.cpp` (goldens re-pinned to `0x6172068cb2eb0ae3` and `0xa26bab6e23ab1fbf`, set only once the reference encoder and the library agreed), `GateSitesTests.cpp` (six sections), the save and day-loop test support, and new `Tests/{WorldSaveDayTests.cpp, DayLoopWorldTests.cpp}`.
- `CMakeLists.txt`.

**Design:**
- **One clock:** the world clock is authoritative, and a valid world has `DayLoop.Now == Clock.Now()`. The save reader refuses any other world.
- **Schema bump:** v7, rather than making `dayloop` optional, so every key stays required and an older build refuses a v7 file as newer rather than as damaged.
- **v6 saves:** still load with a fresh day. Their version-3 hash cannot be recomputed, so they load unverified.
- **v1–v5:** still migrate.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 144/144: 22,738 world save, 1,451,654 world hash, 820,922 day loop and 79,559 Gate checks.

**Integration still to do:** route the reducer's activity and time commands through `ApplyDayCommand`; the Unreal subsystem writing v7 slots and checkpoints; condition, practice and receipts feeding stats and the aptitude projection.

## Checkpoint — first playable front end: title menu and creation flow (no appearance editor), 2026-10-11

Started by a Sonnet 5.5 subagent, which wrote the subsystem additions, the HUD and the Slate screens before a session end cut it off. I finished the wiring, built it and merged it.

**What it is:** C++ Slate, no UI assets, drawn by `APerfectWorldFrontEndHUD` over the block test scene, with the camera turning slowly behind a darkened overlay.
- **Title:** New Game, Continue (the most recently written slot), Load (three slots with their descriptions), Settings, Quit.
- **Creation, in five steps:**
  1. Household cards.
  2. Traits: all 20 with a rank slider each, a "spent / 1,000" bar that changes colour, the five presets, and Reset.
  3. Identity: name, date of birth within the valid window, pronouns, primary language.
  4. Stream: Science, Commerce or Humanities, with an optional reason.
  5. Review: everything, the core's warnings (rank-0 household means institutional care), the opening home, and Start Life.
- **Into the world:** Start Life saves to the first empty slot and enters the block world. Escape opens Resume, Save, Quit to Title and Quit.
- **No appearance editor,** as the owner asked; appearance keeps its defaults.

All text goes through LOCTEXT. Every rule stays in the portable core: the UI shows the core's own messages.

**Files:**
- New: `Source/PerfectWorld/Public/{PerfectWorldFrontEndHUD.h, PerfectWorldCreationViews.h}`, `Private/{PerfectWorldFrontEndHUD.cpp, PerfectWorldCharacterCreatorFlow.{h,cpp}}` and `Private/UI/*` (title, creation flow, household, traits, identity, stream, review, load, settings and pause pages, plus style and widget helpers).
- Changed: `PerfectWorldCharacterCreatorSubsystem.h` (presets, identity, stream and review accessors), `PerfectWorldBlockTestGameMode.cpp` (`HUDClass`), `PerfectWorld.Build.cs` (Slate, SlateCore).

**Exact check so far:** `python Scripts/build_unreal.py --engine-root "D:\Epic Games\UE_5.8"` (D: clone at `79aaa0f` plus these files): **Succeeded**, no warnings or error lines. The packaged build and a visual check come next. Nothing here has been seen running yet.

**Front end, first look in the packaged build (2026-10-11):** `package_windows.py` exit 0 and `BUILD SUCCESSFUL`. Launched windowed at 1600×900 and captured the window.
- The title, household and trait pages render as designed: dark panels with amber accents, the block scene turning behind, the points bar, presets, and sliders with +/− buttons.
- **Fixed:** the title logo was clipped to "PERFECT WO". It is now in a shrink-only scale box.
- **Fixed:** the trait rows' rank and cost label was cut off. It now uses two lines in a wider column.
- **Fixed:** Start Life is irreversible but fired on a single press. A held "accept" from the cloud PC's virtual gamepad (Parsec and ViGEm are installed) carried a test run from Traits straight into the world. Start Life now asks "Begin this life?" with **Not yet** focused by default, so a stray or held accept cancels.

After the fixes, Rung 2 **Succeeded** and the repackaged build boots to the corrected title. The Identity, Stream and Review pages and the pause menu were not walked through on screen, because the owner was using the desktop.

**Released as `v0.1.0-alpha` (prerelease, 2026-10-11):** tag `v0.1.0-alpha` on `28541f6`, https://github.com/debdutta777/Perfect-World/releases/tag/v0.1.0-alpha. The repository is private, so the release is visible only to people with access. The asset is `PerfectWorld-v0.1.0-alpha-Win64.zip` (402,654,098 bytes, sha256 `1b8d854c…f687b1`): the Development packaged build with its `Saved` folder (logs, test saves) left out.

## Checkpoint — core data for the trait screen: presentation, cost queries, atomic edits with undo, 2026-10-11

Built by a Claude Sonnet 5.5 subagent and merged here as a straight copy (its base `0e681da` matches main's code for these files).

**Files:** new `Public/CreationPresentation.h`, `Private/{CreationPresentation.cpp, CreationEdits.cpp}` and `Tests/CreationPresentationTests.cpp`; additive changes to `CharacterCreation.{h,cpp}`; `CMakeLists.txt`.

**What it adds:**
- **Presentation data:** the seven trait groups, plus per-trait descriptions and per-rank meanings with structured effect lines (unit, direction and value transcribed from `CHARACTER_CREATION.md`'s canonical catalogue). Household Resources rank 0 states institutional care.
- **Cost queries:** cumulative cost and incremental cost or refund.
- **Atomic edits:** `ClearAllocation`, `ResetAllocation` and `ApplyPreset` are each one undo step.
- **Undo:** a 32-deep undo and redo stack covering ranks only, refused after Start Life and never saved.
- **Previews:** for presets, clear and reset, each running the real edit on a copy.
- **Unchanged:** the save format, still schema 5.

**Check:** `python Scripts/build_core.py --build-dir D:/pwc_tpres` in the agent's worktree: **100% tests passed out of 145**, no warnings. The same run on main was still in progress at commit time and is recorded separately.

**For review:** the agent wrote the 80 rank meanings (ranks 1–4) itself. The doc gives only effect numbers, so the wording needs a narrative pass.

## Checkpoint — trait rank meanings rewritten for tone and fairness (gpt-6-astra), 2026-10-12

gpt-6-astra (Codex, medium effort) edited only the English strings in `Source/PerfectWorldCore/Private/CreationPresentation.cpp`: the 20 one-line descriptions and the rank meanings. No number, unit, direction, effect subject, rule note, ID or structure changed; I checked the diff for that.
- **Framing:** abilities are lived experience and practice, not innate superiority. Unsupported promises (reflexes, illness, academic results) are removed. Household reserve is kept separate from affection and stability, and caregiver wording includes children's-home staff.
- **Rank 0:** reads "You bring everyday experience, with room to learn as you go." Household Resources rank 0 still states institutional care in a children's home, with no family household and no reserve, written with equal dignity.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm` on `integrate/ui-phase1-lane-week`: **100% tests passed out of 146**.

## Checkpoint — M3-02 week compression and the state diff tool, 2026-10-12

Commit `2606af6`.

**Files:**
- New: `Public/{WeekCompression.h, StateDiff.h}`, `Private/WeekCompression{Plan,Run,Compare}.cpp` and `Private/StateDiff{,Lines}.cpp`.
- New tool: `Tools/StateDiff/main.cpp` (`pwstatediff`).
- New tests: seven test files under `Tests/` (`WeekCompression*`, `StateDiff*`).

**What it adds:**
- **Two resolvers:** `CompressWeek` and `PlayWeekStepByStep` resolve the same plan through `ApplyDayCommand`.
- **Equivalence:** the two must give identical persistent state, field by field, except for the quantities the time contract excludes.
- **Interrupts:** at most one per week.
- **Firsts:** a first-of-kind entry always stops compression for play.
- **Disclosures:** every choice taken by standing order is listed.

**Exact check:** Rung 1 on `integrate/ui-phase1-lane-week`, **146/146**.

**Not yet done:** the week planner (M3-15) does not use it.

## Checkpoint — UI phase 1, Kolkata lane and week compression verified in the packaged build, 2026-10-12

These results are for `integrate/ui-phase1-lane-week` at `662b26f`, built from the D: clone:
- **Rung 1:** `build_core.py`, **146/146**.
- **Rung 2:** `build_unreal.py`, **Succeeded**. The shared PCHs were pre-warmed first.
- **Rung 4:** `package_windows.py`, exit 0, `BUILD SUCCESSFUL`.

**Seen in the packaged build** (windowed, 1600×900):
- **Title:** it renders over the block-built Kolkata lane. The left panel fades from left to right, which confirms the `Orient_Vertical` fix. The save-aware **Continue** shows the slot note beneath it. See `Docs/Images/title-phase1-lane.png`.
- **Household page:** it renders in the new palette and in Hind Siliguri, with the cyan focus outline. See `Docs/Images/household-phase1.png`.

**Not walked through on screen:** the cloud PC's virtual gamepad presses the focused button within seconds of launch, so the rest of the flow went untested on screen. That covers the Identity, Stream and Review pages, the pause menu, and controller navigation.

## Checkpoint — UI phase 2: Traits, Household and Stream, with the review fixes, 2026-10-12

**Who did what:**
- **gpt-6.1-sol (Codex, medium):** wrote phase 2 per `UI_FRONTEND_SPEC.md` §2–4 (`1027ee0`); its report is `Docs/UI_PHASE2_STATUS.md`.
- **Traits page:** a 0–4 rank selector, an inspector comparing the current and proposed rank, a sticky budget strip, a "Choose a starting background" drawer with preview-before-apply, Undo and Redo, and Clear and Reset under More.
- **Household and Stream:** redesigned with a 35/65 and a 60/40 split.
- **Integrator:** dropped Codex's unrequested `STATUS.md` split and audit script.
- **gpt-6-astra (Codex, medium, read-only review):** found no compile or crash defects. It asked for changes:
  - five input and focus defects;
  - the UI computing group totals and the strongest traits itself;
  - the Stream page's missing sample-day and route content;
  - localisation of core strings, which is deferred.
- **Fixes:**
  - **gpt-6.1-sol (`08b8e01`), five input fixes:**
    - stick input reaches the rank selector, and the Traits page owns the stick latches;
    - drawer traversal at the scroll bounds;
    - Escape and B cancel a proposal before leaving Traits;
    - held-key repeats are consumed;
    - the ladder toggle keeps focus.

    The integrator changed one thing: the stick now only scrolls the preview, because analog events arrive every frame.
  - **Claude Sonnet (`0bb8471`):**
    - new core APIs `GroupAllocationTotals` and `StrongestTraits`, which previews now carry;
    - new `StreamPresentation.{h,cpp}` holding the spec's stream copy verbatim: card summaries, four sample-day rows, captions, and the routes and equal-XP text;
    - the adapter and Stream page now read the core;
    - new test `PerfectWorldStreamPresentationTests`.
- **Integrator fixes:**
  - Unreal has a global `ETextRole`, so `using PWUI::ETextRole;` failed to compile; the name is now qualified;
  - the deprecated `EKeys::Virtual_Accept` is dropped.

**Exact checks on `41d599d`:**
- **Rung 1:** `build_core.py --build-dir D:/pwm`, **147/147**.
- **Rung 2:** **Succeeded**, no warnings.

**Not yet verified:** packaged screenshots and a walk-through of the new pages.

## Checkpoint — M4-03 the ten days (portable), 2026-10-12

Built by Claude Sonnet 5.5 in its own worktree and merged as `0c5c0d7`.

**Files:**
- New: `Public/{Countdown.h, CountdownContent.h}` and `Private/Countdown{Content,Rules,Profile,Offers,Run,Text}.cpp`, plus `CountdownInternal.h`.
- New tests: six files `Tests/Countdown*`.

**What it adds:**
- **Span:** from 18:00 on Awakening Day to 06:00 on exam morning, resolved only through `CompressWeek`.
- **Opportunities:** twelve, with mutual-exclusion groups and time-overlap refusal.
- **Odds:** shown before committing, and moved by relationships.
- **Seeds:** stored at commit, so a reload never rerolls. A byte-identical replay is tested.
- **Obligations:** three. Each stands by default and can be negotiated once, with its cost shown first.
- **World event:** one Gate event at a fixed time.
- **Strategies:** study, people and rest end with different but comparable profiles, also across six more seeds.

**Design choices:** 11 places where the docs were silent; each is marked `// Design choice (M4-03):` in the code. Examples: six preparation dimensions mirroring the tower floor types, and a definition of "comparable" as totals within 25%.

**Not yet done:**
- not in `WorldState`, the world hash or the save;
- relationship requests are carried but not applied;
- no institutional-care obligation variant yet.

**Exact checks:**
- **Rung 1:** `build_core.py --build-dir D:/pwm`, **148/148**, `PerfectWorld countdown: 6250 checks passed`.
- **Rung 2:** **Succeeded**.

## Checkpoint — M1-13 walking player and the scale audit, 2026-10-12

Built by Claude Sonnet 5.5 in its own worktree (`7743105`) and merged as `fcb17d9`.

**The player:** `APerfectWorldPlayerCharacter` replaces the flying `ADefaultPawn`.
- Capsule radius 34 cm, half-height 82.5 cm.
- First-person camera on the capsule at 155 cm eye height. The old camera sat 2 m above the road.
- Walk 140 cm/s, run 400 cm/s, step height 45 cm.
- Third person on a 300 cm spring arm, adjustable from 120 to 600 cm. V or gamepad Y toggles it without changing position or facing.
- A block body, hidden in first person.

**Input:** Enhanced Input actions and a mapping context are built in C++ at runtime, with no config edits. `APerfectWorldPlayerController` forces the Enhanced player-input class, and a legacy fallback is logged if it is ever needed.

**Collision:** walls, road, footpath and ground block. Invisible pawn-only barriers seal the gaps between buildings and the alleys. Poles, vehicles and stall counters also block. Decoration does not.

**Scale:** `Docs/SCALE_AUDIT.md` has the full table.
- Stair-room door 70 × 175 → 90 × 210; balcony and residence doors → 90 × 210.
- Windows 120 × 135, sill at 75, with a 45 cm chhajja.
- Railings 100, parapet 100, shutters 240, stall counters 90, poles 900.
- Taxi 436 × 165 × 160; scooter 190 long.

**Exact check:** Rung 2 on `fcb17d9`, **Succeeded** on the first compile.

**Not yet verified:** walking, collision, camera toggling and the feel in play. The owner's desktop was in use, so the game was not launched.

## Checkpoint — M4-04 tower route matrix and scoring (portable), and the walking build packaged, 2026-10-12

**Who did what:**
- **gpt-6.1-sol (Codex, medium):** started M4-04 and was cut off twice, first by a network error and then by the account usage limit. Its partial `Tower.{h,cpp}` and tests were kept.
- **Claude Sonnet 5.5:** finished it (`core/m4-04-tower`).
- **Integrator:** added a `static_assert` and an id test tying `TowerMethod` to M4-03's `CountdownDimension`, and settled the countdown's `TODO (M4-04)`.

**What it adds:**
- **Routes:** nine, three per floor, mirroring M4-10 to M4-12. Every route has a non-combat completion. The one combat-capable route earns the same ceiling.
- **Disciplines:** every one is reachable by at least two routes. Hunter is reached through team transit.
- **Mission clock:** resolved once through `TimeContract`.
- **Defeat:** costs rank at every depth. The score is signed, and defeat ranks below withdrawing.
- **Credentials:** one per aptitude path.
- **Rank:** against a uniform synthetic cohort of 999, to be calibrated at M6.
- **Offers:** at least four at any result, including a retake. The hunter licence is gated by credential, not by combat.
- **Top decile:** every non-combat route combination reaches it at the same quality, about 785 per floor.

**Exact checks:**
- **Rung 1:** `build_core.py --build-dir D:/pwm`, **149/149**, `PerfectWorld tower: 9628 checks passed` (before the integrator's alignment checks were added).
- **Rung 2:** on `e5dfddf`, **Succeeded**.
- **Rung 4:** packaging of the M1-13 walking build at `49d0550` to `D:\PWBuilds`, `BUILD SUCCESSFUL`. The build was not launched, because the owner's desktop was in use.

## Checkpoint — world-entry deadlock fixed; walking verified in the packaged build, 2026-10-13

**The hang:**
- **Symptom:** the packaged walking build froze the moment Continue entered the world. CPU fell to 0% and the frame counter stopped.
- **Diagnosis:** relaunching with `-ini:Engine:[Core.System]:HangDuration=10` made the hang detector log the game thread's stack. Destroying the title screen re-entered Slate's locked accessible-text registry. The cause was `PWUI::MakeContentButton`'s accessible-text lambda, which held a strong `TSharedRef` to the button content.
- **Fix:** `02b0b6b` captures a `TWeakPtr` instead. The trap is recorded in `DEVELOPMENT_WORKFLOW.md` §4.
- **Origin:** phase 2 code. It was not the walking-player code.

**Exact checks:**
- **Rung 4:** `package_windows.py` at `02b0b6b`, `BUILD SUCCESSFUL`.
- **Launched at 1600×900 with the hang detector on:** Continue entered the world and the game stayed responsive.
- **Driven by key events:**
  - W walks;
  - Shift+W runs;
  - D strafes up the 15 cm kerb onto the footpath;
  - V shows the third-person block body, and a second V returns to first person.
- **Log:** confirms Enhanced Input is active.
- **Screenshots:** `Docs/Images/walk-first-person.png`, `walk-third-person.png` and `walk-footpath-stall.png`.

**Seen, and being fixed:** at a stall the footpath is too narrow to pass. The strafe ended pressed against the counter under the awning, as `LANE_LIFE_PLAN.md` §D found.

**Not yet judged:** the movement feel, which needs the owner; the controller; and 20 camera toggles.

**Capture helper:** an earlier capture helper copied screen pixels and photographed another full-screen app that was on top. It also once froze the game through `PrintWindow`. Captures are now taken only while the game window is in the foreground.

## Checkpoint — lane circulation and floor datums, verified in the packaged build, 2026-10-13

Built by Claude Sonnet 5.5 from `LANE_LIFE_PLAN.md` §D and `LANE_LIFE_IMPLEMENTATION.md` §C, and merged as `968f7f7`.

**What it changes:**
- **Stalls:** recessed into 100 cm niches inside the building.
- **Kerb side:** poles and trees sit at the kerb edge, with collision ending at |Y| 279.
- **Pots:** placed only where a setback leaves at least 129 cm.
- **Awnings:** they clear 210 cm above the footpath; zero-setback awnings are held by stay rods instead of poles.
- **Residence thresholds:** run from 0 to 15.5 cm, so the opening is a true 90 × 210.
- **Floor heights:** `FloorZ(n) = 15 + n × StoreyHeightCm` per building, 300 cm by default with 360 cm available. Upper floors rise 15 cm.
- **Collision:** colour rows now carry explicit collision roles. Solid props block the Pawn, Camera and Visibility channels within 5 cm of their visible geometry. The lane boundary stays Pawn-only.
- **Props:** the tank, scooter and taxi are rebuilt to their references.
- **`SCALE_AUDIT.md`:** rewritten, including the work still to track.

**Exact checks:**
- **Rung 4:** at `968f7f7`, `BUILD SUCCESSFUL`, with no compile warnings or errors.
- **Development-only route check:** logs `Lane footpath route: continuous; narrowest clear width 121 cm at X -714 on side -1 (3278 samples)`.
- **In play:** strafed onto the right footpath and walked 7 s along it, past the recessed stalls, without obstruction. The third-person view works under the awnings.
- **Screenshots:** `Docs/Images/walk-footpath-recessed-stalls.png` and `walk-footpath-third-person.png`.

**Still tracked:**
- solid building volumes and interiors;
- filled door and window frames;
- balcony and roof collision;
- navigation geometry;
- visible alley-end architecture;
- the registered door, window and counter assets disagreeing with the lane;
- run speed 4 m/s against the 3.5 m/s reference.

## Checkpoint — Fivefold Aperture and reduced motion (lane life stage 1), seen in the packaged build, 2026-10-13

**Who did what:**
- **gpt-6.1-sol (Codex, medium):** built `LANE_LIFE_PLAN.md` §A, merged as `d27345b`.
  - `UI/PWSigilPainter` and `UI/SPWAwakeningSigil` are procedural Slate (`MakeLines`/`MakeBox`) with geometry cached per size. The widget is non-focusable, hidden from hit tests and excluded from accessibility.
  - The title has one aperture: five equal 48° sectors, open chevrons, calibration ticks with only the outer ring rotating (90 s per turn), and motes.
  - The creation stepper, pause and review screens have seals with the specified pictograms.
  - Reduced motion is a `Config=GameUserSettings` preference with a Settings toggle. It stops the sigils, page transitions, the traits drawer and the title camera drift at once.
  - The Start Life flourish is deferred until start-and-save is transactional (`TODO_StartLifeTransactionalFlourish`).
- **Integrator:**
  - **Exit-crash guard:** the sigil's destructor no longer touches UObjects after shutdown.
  - **Contrast change (`de95ce3`):** a deliberate departure from the plan's 25–45% opacity, for the full title aperture only; seals keep the specified weights. The first packaged capture showed the aperture almost invisible against a sunlit wall. It now sits on a soft dark lens of the Background token, with 2.25 px strokes at 72% amber and brighter ticks and motes.

**Exact checks:**
- **Rung 4:** at `d27345b` and again at `de95ce3`, `BUILD SUCCESSFUL`, with no compile errors or warnings.
- **Seen in the packaged build at 1600×900:** the title aperture (`Docs/Images/title-fivefold-aperture.png`) and the creator step seals on the phase 2 household page (`Docs/Images/creator-step-seals-household.png`).

**Not yet checked:**
- the reduced-motion toggle in use;
- 720p and 150% text;
- the pause and review seals;
- whether astra accepts the contrast change.

## Planning note — next batch, 2026-10-13

**The batch-planning call failed.** The owner asked gpt-6-astra to lead planning. Its call to pick the next batch hit the ChatGPT usage limit, which resets 2026-10-14 00:09. It was not retried.

**The batch instead:** the integrator took the next steps directly from astra's existing `LANE_LIFE_PLAN.md`: stage 3, "One home and one adult".

**Who builds it:** Codex is unavailable, so three Claude Sonnet 5.5 agents run in parallel, each owning separate files.
- **A, portable core:** `Doors` holds door definitions, door state and access refusals. `Conversation` holds the household adult's authored lines, selection by time and remembered facts, and costs charged exactly once.
- **B, Unreal:** `PerfectWorldLaneInteriors`, a walkable, furnished shell for the opening home. `APWDoorActor` is a hinged door. B also owns the shared `IPWInteractable` interface.
- **C, Unreal:** `UPWInteractionComponent`, which handles reach, the view cone, priority, stickiness and an E/X action, plus an interaction prompt, a conversation panel and presenter, and `APWNpcCharacter`.

The integrator then wires the doors and the conversation through the core.

## Checkpoint — one home and one adult (lane life stage 3), walked in the packaged build, 2026-10-14

**Built by:** three Claude Sonnet 5.5 work orders and an integration order. Codex was rate-limited. The batch was chosen from astra's `LANE_LIFE_PLAN.md`.

**Merged:**
- **Core `Doors` and `Conversation`:** 303 and 9,018 checks, 151/151.
- **Unreal:** the opening home shell and furniture, `APWDoorActor`, `UPWInteractionComponent`, the prompt, the conversation panel and presenter, and `APWNpcCharacter`.
- **Integration:** `UPWLaneLifeSubsystem` owns the live `DoorState` and `ConversationState`. Doors and the conversation run through the core, refusals appear as on-screen notices, and the household adult stands in the living room.
- **Integrator fixes:**
  - a missing `class APawn;` in `PerfectWorldInteractable.h`;
  - a unity-build clash: two .cpp files each defined `Fl` and `CeilingW`, now shared as `HomeFloorW` and `HomeCeilingW`;
  - an empty stray file at the repo root, deleted.

**Exact checks:**
- **Rung 1:** 151/151.
- **Rung 2:** at `50b28bb`, **Succeeded**, with no warnings.
- **Rung 4:** `BUILD SUCCESSFUL`.

**In the packaged build at 1600×900, driven by key and mouse events** (the cheat `EnableCheats` + `BugItGo` from the console placed the camera inside the small home):
- **Front door:** "E / X · Open" at the front door. E swings it inward through the core and the prompt changes to "Close".
- **Inside:** walked into the entry; the washroom door opens onto a toilet, ventilator and shower rail.
- **Living room:** a real, furnished room with a table and chairs by the front windows (the street is visible outside), divan, family photo, almirah, rug, ceiling fan and warm lights.
- **Conversation:** "E / X · Talk to Your parent" opens the panel. "Your parent: Breakfast is ready. Tell me what time you expect to be back." The morning line matches the fixed 07:30. Choosing "Around seven." gives "All right. Let me know if it changes." Esc leaves and returns control.
- **Screenshots:** `Docs/Images/home-front-door-opened.png`, `home-living-room.png`, `home-washroom.png`, `home-parent-in-living-room.png` and `home-talk-to-parent.png`.

**Defects found, to fix next:**
1. The living door, open by default in the core, swings its leaf across the household adult's spot and line of sight. Move the adult or the hinge side.
2. The adult appeared to face away from the entering player, although they are meant to face the living door. Check the block body's forward direction against the actor yaw.
3. From the doorway, the adult sits at almost exactly the 2 m reach, so they do not qualify and the door wins the prompt. Place them nearer the threshold.
4. Navigating the small home by scripted keys is hard. Future walk tests should start from `BugItGo` positions.

**Not yet done:** charging conversation time (no clock yet); saving doors and conversation; naming the adult (the owner decides); the care-origin staff counterpart.

## Checkpoint — wave 1 of the lane life batch merged and seen in the packaged build, 2026-10-16

**What merged:**
- **gpt-6.1-sol:**
  - **D:** door passage by direction and residency, offers, Knock, invitations and split windows; 151/151 in its own build.
  - **O:** `ResolveLaneOrigin`, which binds the five households and care to residency and a counterpart.
- **Claude Sonnet 5.5:**
  - **DA:** the door swing checks the leaf every 5 cm or less of tip travel, stops at the last safe angle, and settles through `OnSettled`.
  - **TS:** Shanti Chai Corner.
  - **NB:** pure NPC intent (Work, Travel, Idle, Chat, Sleep, Yield).
  - **HL:** the living door moved and made one-way, `HomeAdultSpot`, seat and newspaper anchors, the stair reservation and shadowed light specs.
  - **CR+CH:** catalogue-driven conversation with per-session charge receipts, `pw.conversation/2` with /1 migration, and household lines that remember the concrete arrangement and claim no result.

**Integrator work:**
- door settlement corrects the core;
- the tea shop is hooked into `BuildFull`;
- the adult stands at `HomeAdultSpot`;
- room lights are shadowed with a 6 cm source;
- NPC block heads get a face on +X (eyes, brows, nose), because a symmetric head did not show which way it faced;
- two unity-build name clashes fixed;
- a CMake merge conflict resolved;
- `LaneOrigin.cpp` moved into the core library.

**Exact checks:**
- **Rung 1:** **154/154**.
- **Rung 2:** **Succeeded**.
- **Rung 4:** at `a4267de`, `BUILD SUCCESSFUL`.

**In the packaged build** (positioned with `BugItGo`):
- **Living doorway:** the adult stands in front of the player, facing them, with a readable face. The prompt reads "E / X · Talk to Your parent" with "Tab / RB for the next". The open living-door leaf lies against the wall. E opens the morning conversation. Screenshot: `Docs/Images/home-parent-at-doorway.png`.
- **Shanti Chai Corner:** frontage, sign band, awning, a 90 × 210 door between barred hatch windows, and outdoor benches. Inside are the 90 cm service counter and the prep area (stove platform and kettle, worktable, shelf, back-room doorway), with a fan and warm lights. Screenshots: `Docs/Images/teashop-front.png` and `teashop-counter-and-prep.png`.

**Open items:**
- the shop's shutter door is not yet in the core: D2 is in progress;
- Farida is not yet placed;
- conversation time is not yet charged: CK+TX is pending.

## Checkpoint — wave 2, first part: door settlement, dialogue catalogues, reading and seats, 2026-10-16

**What merged:**
- **gpt-6.1-sol, D2:**
  - `ApplyDoorSettlement` with the swing side;
  - `pw.doors/2` with /1 migration;
  - `door.nabapara.shanti_chai_shutter` with two opening windows (06:00–11:30 and 15:30–19:00);
  - four care-home doors.
  
  15,460 door assertions passed.
- **Claude Sonnet 5.5:**
  - **CC:** the care worker's catalogue. On duty 07:00–15:00, the evening staff and a handover note afterwards, no second person named, shared-jobs agreements that claim no result, and a word-level tone guard.
  - **CF:** Farida's catalogue. Her day runs preparing, serving, midday close, serving, closing. Tea is a social 10-minute sit charged once per session, not a purchase. The bench line plays only after the player asked.
  - **RD:** the reading presenter and panel, paper-like and paged, with text size and keyboard/controller support. Built-in "The Kolkata Morning Courier" for 2 June 2042, with no real people, politics or religion.
  - **FI:** seat and readable actors. Seated state: a sweep-checked 0.4 s glide, eye at 112–115 cm, look limits, safe stand-up spots, and a third-person sitting pose.
- **Integrator:**
  - settlement goes through the core, and doors restore their saved swing side;
  - the home's divan and table chair seats and the newspaper are spawned from the layout specs;
  - `APWReadableActor::GlobalReadRequested` opens the reading presenter.

**Exact checks:**
- **Rung 1:** **156/156**.
- **Rung 2:** at `97015f8`, **Succeeded**: RD, FI, CC and CF compiled first time.

**Not yet verified:** sitting and reading have not been tried in play; the packaged walk is next.

**Packaged walk at `49d560a` (2026-10-16):**
- **Reading:** E on the table's newspaper ("E / X · Read…") opens the paper-like reading panel. It shows "The Kolkata Morning Courier, Monday, 2 June 2042", Page 1 of 3, A-/A+, Previous/Next and Close. Right turns the page and Esc closes it. Screenshot: `Docs/Images/home-reading-newspaper.png`.
- **Seat prompt:** "E / X · Sit at the table" shows at the table chair. Screenshot: `Docs/Images/home-sit-prompt.png`.
- **Sitting itself is NOT verified.** The cheat teleport `BugItGo` leaves the pawn in ghost (flying) mode, so `TrySit` correctly refused ("You can't sit down right now", not on the ground). A second positioning attempt missed the chair. The next walk-through should test sitting from a normal walking start.

## Checkpoint — NPC movement and Farida's schedule merged, 2026-10-16

**Claude Sonnet 5.5, NM:** `UPWNpcMovementComponent`, built in two parts, each under 500 lines.
- **Route following:** follows the core's travel progress at 140 cm/s, catching up at up to 200 cm/s.
- **Far snap:** happens only when more than 5 m away and unseen; the NPC never teleports through a wall.
- **Local yield:** a raised-hand pose when blocked. A persistent block reports through `OnBlockChanged`.
- **Gestures and chat:** procedural work gestures, presentation only. Chat holds the NPC and resumes afterwards.
- **Polite declines are reachable:** the verb stays "Talk to <name>", and E shows the decline.

**gpt-6.1-sol, S:** Farida's `npc_schedule` content.
- **Data:** schema, typed loading and the JSON at `Data/Content/npc_schedule/npc_schedule.farida_qadri.json`.
- **Validation:** weekly coverage, both serving windows, and route-length travel budgets at 140 cm per game minute.
- **Tests:** 23,530 assertions.

**Integrator:**
- registered the schema and its semantic validation in the shared registry (CAT);
- fixed the schedule test's registry path, which must be `<type>/<id>.json`, and made the test print registry issues on failure;
- resolved two CMake merge conflicts.

**Exact checks:**
- **Rung 1:** **157/157**.
- **Rung 2:** at `031fde3`, **Succeeded**: NM and S compiled first time.

**Not yet done:**
- Farida is not yet placed or driven by her schedule;
- the clock does not run yet (CK+TX is in progress).

## Checkpoint — Farida at work, the children's home, and the core clock, 2026-10-17

**What merged:**
- **gpt-6.1-sol, FA:**
  - `UPWContentSubsystem` loads the staged `Data/Content`, via `+DirectoriesToAlwaysStageAsNonUFS=(Path="../Data/Content")`, verified against UE 5.8's staging code. All 149 JSON files are staged in the package.
  - Farida's schedule drives her intent through `NpcBehaviour` into `UPWNpcMovementComponent`, along the tea shop's anchors and routes.
  - The `ShopProprietor` outfit.
  - A conversation catalogue and availability per person.
- **Claude Opus deep-work, CK+TX:**
  - due-now events drain without advancing the clock;
  - a capped advance stops at the last fired event;
  - `conversation.open/choose/leave` charge `CostMinutes` exactly once per `(ChoiceId, Session)` receipt, all-or-nothing;
  - `door.set` and `door.settle` commands;
  - per-person `Conversations` and `Doors` in `WorldState` (not yet in the save or hash; v8 is next).
  
  Checks: 4,684 lane-transaction checks and 17,073 clock checks.
- **Claude Sonnet 5.5, CI:**
  - The children's home: first row, -Y side, Index 1, X -1400 to -300.
  - Rooms: an entry with a staff desk, noticeboard and stair reservation; a common room with study and dining tables; a dormitory with five beds and the resident's own bed, padlocked trunk, tin box and photograph; a washroom.
  - Four doors, a staff spot, seat, readable and storage anchors, and shadowed lights.
- **Integrator:**
  - hooked the children's home into `BuildFull`, and placed its on-duty care worker and seats;
  - resolved a CMake conflict;
  - deleted a stray scratch file an agent's shell had written into the Git install folder.

**Exact checks:**
- **Rung 1:** **158/158**.
- **Rung 4:** at `c776b07` (Farida) and at `962aad2` (CK and the children's home), `BUILD SUCCESSFUL` both times, with no compile errors.

**In the packaged build** (positioned with `BugItGo`):
- **Farida:** she stands behind her counter in her apron, making the serving gesture, at 07:30. "E / X · Talk to Farida" opens: "Good morning. The first pot is ready, if you'd like a cup." The choices are "Sit down for a cup of tea · 10 minutes", "Not today, thank you." and "May I move the bench a little?". Screenshots: `Docs/Images/farida-at-counter.png`, `farida-talk.png`.
- **Care worker:** in the common room. "Good morning. Breakfast is being set out. Tell me what time you expect to be back, and I will put it in my note for the evening staff." The shared-jobs talk costs 3 minutes. Screenshot: `Docs/Images/care-worker-talk.png`.
- **Dormitory:** the resident's bed with its personal objects, and the "Sit on your bed" prompt. Screenshot: `Docs/Images/care-dormitory-own-bed.png`.

**Still open:**
- **Clock bridge:** the clock does not run in Unreal yet. The bridge (BR) must hold a `WorldSim`, drive `time.advance` from free roam, and route conversations and doors through the new commands.
- **Not yet built:** world save v8 for conversations and doors; the care-origin start placing the player in the children's home; the care worker's duty schedule; authored text for the care-home readables.
- **Not yet verified:** sitting from a normal walk.

## Checkpoint — clock bridge, care content, origin-aware start, and C: disk recovery, 2026-10-17

**What merged:**
- **Claude Sonnet 5.5, BR:** one `WorldSim` owns lane time.
  - The free-roam clock runs at 60 real seconds to 60 game minutes. It holds for menus, conversation, reading and an unfocused window.
  - Partial and settle handling.
  - Conversations and doors go through the core commands, so a paid choice moves the clock by its cost.
  - A clock readout.
- **Claude Sonnet 5.5, content:**
  - The care worker's duty schedule: 07:00–15:00 every day with a handover at the desk, plus a weekend variant. A generic `LoadNpcSchedule` loads it.
  - The noticeboard and exercise-book texts.
- **gpt-6.1-sol, EN:**
  - Transactional Start Life: a failed save restores the draft.
  - A `GetLaneOrigin` view.
  - Placement by origin: inside the family home's entry, or beside the resident's own bed in the children's home dormitory.
- **Integrator:**
  - a new life resets the lane world;
  - door residency asks the current life's resolved origin and fails closed, replacing the synthetic Parent plan.

**C: drive incident:**
- **Symptom:** C: reached 0.09–1.5 GB free. Packaging failed with C1060 out of heap, LNK1108 cannot write temp, then LNK1102 out of memory.
- **Silent cook failures:** two failed links left the core editor DLL at 0 bytes and then 4 MB. UBT still treated it as up to date, so the cook quit after 14 seconds with no log.
- **Moved off C:,** nothing deleted:
  - 70 old worktrees removed or archived to `D:\PWArchive`;
  - old scratch files;
  - Unreal's Zen data and local DDC, now junctions to `D:\UnrealDDC`;
  - Codex replay data, now junctions to `D:\CodexData`, with the owner's approval.
- **Build settings:** build temp goes to `D:\PWTemp`, and `MaxParallelActions` is capped at 4.
- **Owner asked to:** move the Windows page file (system-managed on C:, about 4.3 GB) to D: and restart.

**Exact checks:**
- **Rung 1:** at `0d4d90a`, **159/159**.
- **Rung 2:** **Succeeded** after the forced relink.
- **Rung 4:** `BUILD SUCCESSFUL`, with the cook exiting 0.

**Not walked in play yet:** the owner's Eastern Era Resource Editor was open, so the game was not launched (it would take focus and send keys). Pending checks: the clock readout, a 10-minute tea with Farida moving the clock, sitting, the care-origin start, and door residency.

## Checkpoint — world save v8 merged (conversations and doors persist), 2026-10-17

**What merged:**
- **gpt-6.1-sol, SV:** world save schema v8 and world hash v5.
  - The world record gains `conversations` (each person's canonical `pw.conversation/2` text) and `doors` (`pw.doors/2`).
  - It refuses an unknown person, invalid module state, a mismatched open-session person, or a second open session.
  - v7 and v6 files still load, unverified, with no conversations and the authored door defaults; v6 also gets a fresh day.
  - `SerializeWorldSave` returns an empty string for an invalid lane authority. Unreal does not write world saves yet, so no caller is affected today.
- **Integrator:**
  - Found the `PerfectWorldDayLoopTests` crash (0xC0000409, no output): the v6-downgrade block in `DayLoopWorldTests.cpp` still searched for `"schemaVersion": 7`. `find` returned npos, `std::string::replace` threw `out_of_range`, and nothing caught it.
  - The test now builds the strings from `WorldSaveSchemaVersion` and `WorldHashVersion` and checks for them first, so the next version bump fails a check instead of aborting. The day-loop test passes with 820,923 checks.

**Exact checks:**
- **Rung 1:** at `c3cefe1`, **159/159**.
- **Rung 2:** **Succeeded**.
- No package or walk was made for this step: it changes only the portable save, which the game does not call yet.

**Next:** two Claude Sonnet agents are running.
- **AN:** Anindita Sen's schedule data and conversation in the core.
- **SS:** the stationery shop building in Unreal, with Anindita at her counter.

Later rounds (gpt-6.1-sol after the ChatGPT reset):
- schedule-driven presence for any `npc_schedule` person (the care worker, Anindita);
- Mira Sen.

## Checkpoint — packaged walk of the clock, Farida's tea and the care worker; astra's upgrade plan, 2026-10-17

**Walked in the Rung 4 package built at `0d4d90a`.** It contains the clock bridge, the origin-aware start and residency.
- **Clock readout (top right, "Mon 2 Jun · 07:37"):**
  - It runs at one game minute per real second: 07:37 became 08:48 over about 71 s.
  - It holds while a conversation is open: 10:11 held for 5 s.
  - Choosing "Sit down for a cup of tea · 10 minutes" with Farida moved it to exactly 10:21, and it held again afterwards.
  - Screenshot: `Docs/Images/farida-tea-clock.png`.
- **Farida:** she is at her counter at 10:11 with the three choices; after tea, "Here you are. Mind the cup, it's hot. There's no hurry."
- **Children's home:** the care worker is on duty at 10:27, talkable, and at another spot 12 minutes later. She first stood in the common-room doorway and the player had to go around her; LL-02 plans doorway yielding.
- **Continue start:** a clean launch puts the player inside the family home's entry, by the coat and shoe racks, with the living-room doorway open to the right. Interacting gives no obstruction.
  - The morning's "Something is in the way" at `door.nabapara.home_front` was a test-harness artefact. The launch script typed `EnableCheats` while the game was still loading, so the letters reached the game as movement and interact keys.
  - The script now waits for the game to be ready and has a `-NoCheats` mode.

**Not verified in play:**
- **Sitting from a normal walk.** It is covered by the `PerfectWorld.Furniture` automation (started, occupied, obstructed, busy, wrong level), but the scripted keyboard walk did not reach a seat.
- **The care-origin start.**
- **Transactional Start Life.**

**Plan:** `Docs/LANE_LIFE_UPGRADE_PLAN_2026-10-17.md` is gpt-6-astra's plan for this round, with five work orders:
- **LL-01, safe entry and seats:** integrator.
- **LL-02, one schedule presenter with doorway yielding:** gpt-6.1-sol, after Anindita's content (AN) merges.
- **LL-03, parametric interior kit, with the stationery shop as its public variant:** Claude Sonnet, running.
- **LL-04, dawn title view and recomposed title:** Claude Sonnet, running.
- **LL-05, measured scale and circulation:** integrator.

Earlier today a gpt-6.1-sol run was refused for the account's usage limit and was not retried. The limit has since reset, and astra's planning run succeeded.

## Checkpoint — Anindita's content and the dawn title merged; this machine's clock jumps, 2026-10-17

**What merged:**
- **Claude Sonnet, AN:**
  - Anindita Sen's `npc_schedule`:
    - stock check from 07:30;
    - counter 08:00–13:00;
    - meal closure at home;
    - counter 15:00–19:30;
    - reconciliation at the packing desk;
    - home by 21:00, asleep from 22:30.
  - The loader's frozen spec table gains her entry, and the care worker is unchanged.
  - Her conversation catalogue, with two 10-minute help choices charged once per session, and a remembered line after helping with the labels.
  - Three new test executables, including a world save v8 round trip mid-conversation.
  - Route lengths are provisional until the stationery shop (LL-03) is built.
- **Claude Sonnet, LL-04 (1):**
  - A dedicated dawn title viewpoint over the lane (`APWTitleView`). It drifts slowly and stops entirely under reduced motion.
  - While the title is up, the pawn is hidden, never moved or turned. The view is dismissed only after placement succeeds.
  - The title composition is responsive: the menu sits on a dark surface on the left, and the aperture takes the right third (a ring of about 336 at 1280×720 and about 410 at 1920×1080). Ornament is suppressed before controls compress.
  - Not yet walked: the framing and contrast are calculated, not measured.

**Exact checks:**
- **Rung 1:** at `66cd0ea`, in a fresh build directory, **162/162**.
- **Rung 2:** **Succeeded**. It recompiled the whole core and all three game-module unity blobs.
- **LL-04's automation** (`PerfectWorld.TitleView.*`) is compiled but not run.

**Trap: the system clock jumps.**
- In one afternoon this machine's clock read 13:50, then 22:05, 18:18 and 17:10. `w32tm` reports NTP sync, but something keeps moving it, probably the VM host's time sync.
- After a backwards jump, MSBuild and UBT treat new sources as older than their outputs and skip the rebuild.
  - The incremental `D:/pwm` run failed `PerfectWorldActivitiesTests` ("the real content validates") because the core library still held the old schedule loader, which refuses Anindita.
  - A test binary rebuilt with a probe did not contain the probe's text.
- Until the owner fixes the clock, run Rung 1 in a fresh build directory, and check that Rung 2 lists the changed modules among its compile actions.

## Checkpoint — interior kit, NPC presenter and start acknowledgement merged; first headless automation run, 2026-10-17

**What merged:**
- **gpt-6.1-sol, LL-02 (1):** `UPWNpcPresenceSubsystem`.
  - One record per person, presented from `npc_schedule` at the lane clock's instant.
  - Arrivals and departures happen out of view. A missing binding gives a diagnostic and never a default spawn.
  - Provider-based lane bindings, including the children's home's.
  - Doorway yielding with hysteresis.
- **Integrator hooks:**
  - The presenter is initialised with the lane life, reconciled after every accepted command, and reset with the world.
  - Conversation availability comes from the presenter for its people.
  - **The care worker now works to her schedule on the presenter** instead of a static spawn.
  - Farida keeps her own path: the generic loader did not know her schedule yet (fixed in 2a, below).
- **Claude Sonnet, LL-03 (1):**
  - The portable parametric interior layout: compact residence, shop with stockroom and shared entry. Doorways are 90×210 clear, circulation 120, with door sweeps, mirroring and 22 refusal codes; a footprint sweep tests 7,896 layouts.
  - The Unreal kit presenter.
  - **The stationery shop** at the +Y index 2 frontage (X −100…600): sales room, counter with staff gap, stock room, packing desk, and the anchors Anindita's schedule names.
- **Claude Sonnet, LL-04 (2):** a quiet start acknowledgement, played only after a fresh life is saved and placed. All sectors expand about 12% and fade over 450 ms. Nothing plays under reduced motion, and it never delays control.

**Exact checks:**
- **Rung 1:** at `b01c603`, in a fresh directory, **163/163**.
- **Rung 2:** **Succeeded**. LL-03's tests first failed to compile (`Num()` on `std::vector`) and were fixed at `a902a39`. Rung 2 also succeeded at `a5a68e7`, with every game-module blob recompiled.
- **Headless automation (`UnrealEditor-Cmd -nullrhi`), the first run of these suites:**
  - **Pass:**
    - `PerfectWorld.InteriorKit` (3/3);
    - `PerfectWorld.TitleView` (4/4);
    - `PerfectWorld.StartAcknowledgement` (4/4), after a test fix: Slate caches a bound visibility until a prepass;
    - `CareHome`, `Doors` and most of `Furniture`, `NpcMovement` and `TeaShop`.
  - **Fail, nine tests:**
    - five of the new `PerfectWorld.NpcPresence` tests (missing bindings in their setup, and Farida's schedule not loadable by the generic loader);
    - `Furniture.RefusesOccupiedAndObstructedSeats` ("another level refused");
    - `NpcMovement.OnlyFarUnseenPeopleAreMovedAndNearOnesWalk` and `NpcMovement.BlockedPeopleWaitVisiblyAndNeverTeleportThroughWalls` (the far, unseen person is not moved);
    - `TeaShop.Build` ("blocking boxes were recorded").
  - The four older suites had never been run before (STATUS records only `PerfectWorld.Core`), so these are long-standing, not regressions of this round.
  - The earlier statement that sitting is "covered by the Furniture automation" was too strong: one of its cases fails.

**Why the sessions keep ending:**
- The event log shows `qemu-ga.exe has initiated the power off of computer TRUECLOUD`. The cloud provider powers the VM off from outside, while Windows' AC sleep setting is "never".
- Each boot then starts 5 h 30 min off (local time read as UTC) until NTP corrects it. That is the clock jump behind the stale incremental builds.
- Both are for the owner: the provider's auto-shutdown setting, and the VM's real-time-clock time zone.
- Long jobs now run detached (`D:\PWWork\run_codex.ps1`, `D:\PWWork\run_automation.ps1`), so a restart does not kill them.

## Checkpoint — LL-02 (2a) merged: Farida on the generic loader, the stationery door, leaving after closing; the VM's 12-minute power-offs, 2026-10-17

**What merged:**
- **gpt-6.1-sol, LL-02 (2a):**
  - `LoadNpcSchedule` now loads Farida's authored schedule. Tests show it agrees with `FaridaSchedule` at every minute across eight days.
  - Anindita's routes and travel minutes now come from the built shop. The ten travel totals are 8, 6, 5, 23, 14, 13, 15, 10, 28 and 27 minutes.
  - `door.nabapara.stationery_front` joins the authored door set, closed by default and open in her hours. Historical ten-door `pw.doors/2` text migrates, keeping its doors' states, and the new door is added closed.
  - `DoorDirection::Exit`: a person inside may leave a business after closing, while entry stays refused.
- **Integrator:**
  - A v8 world save written before the new door migrates on load, so it no longer hashes as stored. `HashWorldStateWithDoorsText` now authenticates the stored checksum over the door text as stored: tampering is still refused, and the changed world loads unverified.
  - The world-hash goldens were re-pinned from the independent reference encoder (`PW_PRINT_GOLDEN=1`). They change only because the default door text gained an eleventh entry; `WorldHashVersion` stays 5.

**Exact checks:**
- **Rung 1:** on `core/presence-2a` at `8e4f0a6`, **164/164**. The run was incremental in `D:/pwc2a_j`, with every changed file's timestamp refreshed first.
- **Rung 2:** for the merge (`24308c1`), **pending**. The automation debugging agent holds the Unreal build slot on the D: clone.

**Power-offs:** the host's guest agent (`qemu-ga`) powered the VM off at 17:57, 18:08 and 18:21, about every 12 minutes, so any longer job died. None has happened since 18:21, while the session keeps the computer awake.
- Long jobs run detached and write `.done` files.
- Builds stay incremental with refreshed timestamps instead of fresh directories.
- The owner should check the provider's auto-stop or idle settings.

**Running:**
- **gpt-6.1-sol, LL-02 (2b):** Farida onto the presenter, Anindita's stationery bindings and spawn, and the door bridge's Exit direction.
- **A debugging agent:** the nine automation failures.

## Checkpoint — Farida and Anindita on the presenter, the stationery shop staffed, and every related automation test passing, 2026-10-17

**What merged:**
- **gpt-6.1-sol, LL-02 (2b):**
  - Farida moves onto the NPC presenter. Her LaneLife bridge, seven fields and conversation special case are gone, and availability now comes from the generic intent.
  - Anindita's stationery bindings: anchors, the three built routes, her off-lane home, and doorway zones with waiting points. She is registered as a scheduled person when the shop exists.
  - Door commands use `DoorDirection::Exit` when the operating pawn stands on a door's authored inside, so anyone inside can leave a business after closing while entry stays refused.
- **Claude deep-work agent, finished by the integrator:** fixes for all nine failing automation tests. In every case the test was wrong and production was right:
  - **Test worlds never ticked:** `TimeSeconds` stayed 0 with `LastRenderTime` 0, so every actor counted as recently rendered. The fixtures now advance time.
  - **Presence test order:** the lane now reconciles at start, so presence tests bind providers before registering a person.
  - **Overlapping spawn:** the "upstairs" sit test spawned its player inside the first one and was pushed up, falling.
  - **Tea shop count:** the blocking-box threshold was off by one; it is now an exact count.
  - **Content pins:** `ContentLoadsInEngine` now pins 143 files and 179 records.

**Exact checks, at `34fb4d4`:**
- **Rung 2:** **Succeeded**, recompiling the core and all three game blobs after refreshing the changed files' timestamps.
- **Headless automation:** **49/49 Success** across `NpcPresence`, `NpcYield`, `InteriorKit`, `TitleView`, `StartAcknowledgement`, `NpcMovement`, `CareHome`, `Doors`, `Furniture`, `TeaShop` and `Core`.
- **Rung 1:** the core is unchanged since `core/presence-2a` (164/164).
- **Not yet walked in a package:**
  - Farida's afternoon return;
  - Anindita opening at 08:00, her meal closure, and the stock-room route through the staff gap;
  - doorway yielding;
  - leaving the shop after 19:30;
  - the dawn title and the start acknowledgement.

**Housekeeping:** the D: build clone's index had been loaded with an old tree that staged hundreds of deletions; the cause is unknown. Its working state is kept in a stash ("D-clone index/worktree anomaly…") and the clone is clean on `integrate`.

## Checkpoint — packaged walk of the staffed lane: Anindita present but pinned behind her counter, 2026-10-17

**Package:** Rung 4 at `34fb4d4`, `BUILD SUCCESSFUL`, built at 22:19.

**Walk.** Positions were read with the console command `getall CapsuleComponent RelativeLocation`, which logs every capsule. It is more reliable than steering a camera by script.
- **Entry:** the player enters at 07:38 in the family home's entry and the clock runs.
- **Stationery shop:** it is built and enterable, with wall shelves of coloured exercise books, a window display of pens and a hinged front door ("Open").
- **Four NPCs exist:**
  - Farida at her counter, (900, 957);
  - the household adult at home, (220, −690);
  - the care worker in the common room, (−854, −752), moving slowly;
  - Anindita inside the shop.
- No missing-binding or registration diagnostics appear in the game log.

**Finding: Anindita does not reach her counter.**
- At 08:42 she was creeping −U at a few centimetres per real second, along local V ≈ 527 in the stock room.
- From about 09:30 to 10:06 she sat at local (250, 481), pressed against the sales and stock partition (V 435–447 plus her 34 cm capsule radius), directly behind the counter anchor at (250, 325).
- The partition's only opening is at U 568–662.
- The route resolver reverses routes correctly. The likely fault is in following them: a lagging actor chases the moving target, or the next anchor, in a straight line.
- The Claude deep-work agent is on it (`fix/npc-route-following`), with a reproducing two-room automation test.

**Not yet seen:** her meal closure, leaving after closing, doorway yielding, the dawn title in the package, and the start acknowledgement.

## Checkpoint — NPCs keep their pace and finish along their routes, 2026-10-18

**What merged (`530e743`), from the Claude deep-work agent:**
- **People crawled at real frame rates.** `UPWNpcMovementComponent` ticked at a fixed 1/30 s and applied movement input only on its own ticks, while the character movement braked on every frame without input.
  - At 120 fps, Anindita walked 15 of 635 cm in her five-minute travel window. The care worker was slow for the same reason.
  - The mover now ticks every frame while walking. Decisions stay at 10 Hz.
- **A Travel route was dropped when the intent became Work.** A lagging person then walked straight at the anchor, into the stationery partition: 156.2 cm off, 0.1 cm from the wall, exactly the packaged sample at local (250, 481).
  - The route is now kept as a finish route and followed to the anchor.
  - The look-ahead is fixed at 50 cm, so corners are cut by at most about 18 cm.
- **New tests:** `PerfectWorld.NpcRoute` (two tests, on a fixture with the shop's partition and off-axis doorway). Both fail on the old code.

**Exact checks:**
- **Rung 2:** succeeded.
- **Headless automation:** **51/51 Success** (`NpcRoute` 2, `NpcPresence` 5, `NpcYield` 2, `InteriorKit` 3, `TitleView` 4, `StartAcknowledgement` 4, `NpcMovement` 8, `CareHome` 6, `Doors` 3, `Furniture` 5, `TeaShop` 5, `Core` 4).
- **Rung 4:** at `530e743`, `BUILD SUCCESSFUL`, and the game target recompiled all three blobs.
- **Packaged walk: not run yet.** The owner's browser window was open, and the game is not driven while the owner's windows are on screen.

**Known limit:** a person sent to a far anchor with no Travel route still walks straight. That happens on a fresh spawn away from the anchor, or a paid skip from one stay into another; only the unseen teleport beyond 5 m covers it. It needs path data from the presence bindings.

## Checkpoint — LL-03 (2): the frontage inventory and three more enterable ground floors, 2026-10-18

**What merged (`b1ba237`):**
- **Claude Sonnet, LL-03 (2):**
  - `Docs/LANE_BUILDING_INVENTORY.md` covers the lane's 19 near side frontages (10 on −Y, 9 on +Y) plus 2 end masses. 4 were hand-built before this.
  - A pinned portable frontage catalogue (`LaneFrontageCatalogue.h`).
  - The first kit-built batch, all with open doorways until their doors are migrated in the core:
    - `building.nabapara.caretaker_flat` (−Y, X 2600–3300, compact residence);
    - `building.nabapara.tall_flats` (+Y, X −1300 to −200, shared entry with a stair passage);
    - `building.nabapara.tea_row_house` (+Y, X 1400–2100, mirrored compact residence).
- **Integrator hook:** catalogue-pinned frontages build their ground floor with the interior kit (`BuildFull`).
- **Still sealed:** 12 side frontages and the 2 end masses.
- **Still to do for these three:** doors (each needs a core door-set migration), shop furniture sets, and walks in both cameras.

**Exact checks:**
- **Rung 1:** 165/165 on the branch (fresh directory). This adds `PerfectWorldLaneFrontageCatalogueTests`, which ports the row generator and proves every pin, overlap and clearance.
- **Rung 2:** at `b1ba237`, **Succeeded**. The first compile of the frontage code recompiled `LaneFrontageCatalogue.cpp` and all three game blobs.
- **Headless automation:**
  - The full run passed 53 tests with no failures, including `KitFrontages` 4/4. Its log ends mid-line after the second `TitleView` test.
  - A separate `TitleView` run passed 4/4.
  - So all 55 tests pass across the two runs; the first run's log did not record its last two.
- **Packaged walk:** still pending. That covers the route fix and these frontages.

## Checkpoint — persistence (A): Unreal slots read and write world save v8, 2026-10-18

**What merged (`ffe6129`), from gpt-6.1-sol:** `PerfectWorldLaneLifePersistence.h/.cpp`.
- `CaptureWorldSave` builds a v8 save from the creation record, the lane `WorldSim` and the player's placement, and refuses an invalid snapshot rather than writing an empty file.
- `WriteWorldSlot` uses the existing temp-and-backup protocol, so a failed write keeps the previous usable slot.
- `ReadWorldSlot` validates fully before anything live changes. It reports the source version, whether the hash verified, and whether the save migrated.
- A save made mid-conversation closes the conversation on load and keeps its memory and payment receipts, so nothing is charged twice.
- The restore checklist for wiring this into the game is in `Docs/PERSISTENCE_HANDOFF.md`. Wiring is submission B, and it alone makes a restart actually keep the world.

**Exact checks:**
- **Rung 2:** at `ffe6129`, **Succeeded**.
- **Headless automation:** **60/60 Success** in one run, including `PerfectWorld.Persistence` 5/5:
  - `RoundTrip`;
  - `InterruptedWrite`;
  - `RefusesDamageWithoutMutation`;
  - `LegacyCreatorAndWorldVersions`;
  - `BackupRecoveryAndRepeatedSave`.
- **Rung 1:** sol ran 164/164 on its branch before the frontage catalogue merged; the core is unchanged by this order.

## Checkpoint — Mira Sen in the core, 2026-10-18

**What merged (`bf50f20`), from Claude Sonnet:**
- **Mira Sen's seven-day `npc_schedule`:**
  - morning study at home;
  - school 09:50–15:50 (`location.nabapara.school`, off-lane);
  - Tuesday and Thursday 16:30–18:00 serving at `anchor.stationery.packing_desk`, never on Anindita's anchor at the same minute;
  - her own afternoons on Monday, Wednesday and Friday (`location.nabapara.mira_out`);
  - lighter weekends;
  - evening study, and sleep from 22:30.
- **Her conversation:**
  - the plan's lines, word for word;
  - a remembered line only after the notes are returned;
  - two 10-minute choices charged once per session;
  - Busy and Asleep decline nodes.
- **Tests:** three executables (schedule, conversation, world save v8 round trip).
- **Engine:** the content pins move to 144 files and 180 records.
- **Gap:** "Give back the notes" is always offered at the desk, because the core catalogue cannot see `thread.borrowed_notes`.

**Exact checks:**
- **Rung 1:** at `bf50f20`, **168/168**. The run was incremental in `D:/pwm3`, with the changed files' timestamps refreshed.
- **Rung 2:** **Succeeded**.
- **Headless automation:** **60/60** in one run.
- **Not in Unreal yet:** Mira needs a packing-desk spot from the stationery provider, and her off-lane legs must enter and leave at the shop door.

**Running:**
- gpt-6.1-sol on persistence (B): Start, Save, Load and Continue on the v8 slots.
- gpt-6-astra planning the next round, as the owner asked.

## Checkpoint — persistence (B): Start, Save, Load and Continue use world-save-v8 slots; sol's upgrade plan, 2026-10-18

**What merged (`02c9395`), from gpt-6.1-sol:**
- **Start** writes a fresh v8 world transactionally.
- **Save** (pause) captures the clock, door swings, conversation memory and receipts, and the player's placement. A failure is reported in the UI.
- **Load and Continue** validate before anything live changes, then:
  - close saved conversations without a second charge;
  - restore doors and swings;
  - reset NPC presentation once;
  - restore placement if it is safe, otherwise fall back home.
- **New tests:** `PerfectWorld.PersistenceFlow` (4).

**Plan:** `Docs/UPGRADE_PLAN_2026-10-18.md` is gpt-6.1-sol's upgrade plan (UP-01..06), for Claude Sonnet subagents, at the owner's request.
- **Running:** UP-04 (talkability and person heights; finished, not merged), UP-05 (stairs), UP-06 (creation and pause).
- **Next:** UP-01 and UP-02 (Mira in Unreal) and UP-03 (Subrata) wait on NL-01 and NL-03.

**Exact checks:**
- **Rung 2:** at `02c9395`, **Succeeded**. The first attempt stalled: three `cl.exe` processes used about 8 s of CPU in 35 minutes while four agent builds competed for the machine. They were ended, and the rerun succeeded.
- **Headless automation:** **64/64 Success** in one run.
- **Not run yet:** a packaged restart (quit, relaunch, Continue). It needs the owner's windows closed.

## Checkpoint — Batch A pieces merged: furniture sets, heights, 16 doors with knocking, Mira's bindings, stairs and richer creation and pause, 2026-10-19

**What merged (integrate at `0329a19`):**
- **NL-03:** general-goods and workshop furniture sets.
- **UP-04:** per-person authored heights and the `NpcRoster` talkability gate. Presence spawn clearance now uses the person's height.
- **NL-01 (Claude deep-work):** a 16-door roster with private-dwelling access (knock, visiting hours, host home) and unrestricted exit.
  - 10- and 11-door v8 saves migrate. Real frozen fixtures, and the generator that made them, are in `Tools/DoorFixtures/`.
  - The hash goldens were re-pinned.
- **NL-04 (Claude Sonnet):**
  - The three kit front doors and the two tall-flats unit doors are hung.
  - Each door's two-way flag comes from the core.
  - The LaneLife bridge handles knocking (`PerfectWorldLaneLifeKnock.cpp`).
- **UP-01:** Mira's bindings. School and her own afternoons are off-lane behind the shop threshold. A standalone probe walked all 30 journeys with no wall crossing.
- **Integrator:** Mira is registered with the presenter.
- **UP-05:** a stair layout contract and an isolated test fixture.
- **UP-06:** richer creation and pause screens.
- **Fixed:** the `ShopFurniture` test expected 7 anchors, but each set publishes 6 by design.

**Exact checks:**
- **Rung 2:** at `0329a19`, **Succeeded**.
- **Headless automation:** 83 found, **80 passed, 3 failed**:
  - `CreationPause.CreationHierarchyAndProgressionHoldAtEverySize`: the header wraps at 150%-scaled sizes;
  - `CreationPause.PauseFitsEveryWindowAndScale`: the pause card overflows at 853×480;
  - `LaneStair.Walk`: about 23 cm of headroom to the slab against 25 cm required.
  - These went back to their authors, who are fixing them.
- **Rung 1:** for this merge, interrupted by power-offs and not completed.

**Power-offs:** the host powered the VM off at 18:29, 19:14, 22:32 and 22:59 on 2026-10-18. Build jobs no longer refresh timestamps on a rerun, so an interrupted build's objects are kept. The owner has asked to favour pushing and delegating over long builds.

## Checkpoint — UP-02 (Mira's arrivals), AW-01 (the scenario driver) and the stair and UI fixes compiled; 87/95, 2026-10-18

**What merged and compiled, at `28f8b29`:**
- **gpt-6.1-sol, UP-02:** off-lane arrivals reconstruct the authored journey and appear unseen at the threshold, then walk in. Departures walk out to the threshold before unseen removal. Eight `MiraPresence` tests.
- **Claude Sonnet, AW-01:** the non-Shipping scenario driver (`-PWAcceptance`). It walks the real pawn and writes an atomic result per step, with screenshots. Four `Acceptance` tests.
- **Claude Sonnet, fixes:**
  - UP-05: stair headroom volumes reach back two capsule radii.
  - UP-06: a 22 pt creation header and a tighter pause card.

**Exact checks:**
- **Rung 2:** **Succeeded**. This was the first compile of AW-01 and UP-02.
- **Headless automation:** 95 found, all completed, **87 passed, 8 failed**.
  - **Fixed:** `LaneStair.Walk` and the creation header.
  - **Still failing:**
    - `CreationPause.PauseFitsEveryWindowAndScale`: the pause card still overflows 853×480.
    - Six `MiraPresence` tests: "walk reaches the authored endpoint".
    - `NpcRoster.SpawnedPeopleAreTalkable`: Mira is not physically present at Tuesday 17:00.
  - All of these went back to their authors.

## Checkpoint — Batch A compiles and passes: 100/100 engine automation, 2026-10-19

**What merged since 87/95:**
- **gpt-6.1-sol:**
  - AW-02, the acceptance runner: a private `-UserDir` plus verified save backups with a restore journal, and evidence checks. Self-test 41/41.
  - The UP-02 test fix.
- **Claude Sonnet:**
  - UP-06 fix 2: a two-column compact pause card, measured at 792×262 of 821×448.
  - UP-03a: Subrata's portable life (schedule, conversation, tests).
  - UP-03b: Subrata's bindings and spawn.
- **Claude deep-work:** NPC polylines collapse exact reversals.
  - Mira's walk-in ran out to the counter hub and straight back along the same line. The 50 cm look-ahead then folded back on itself and she oscillated 298.7 cm from her desk.
  - The same latent bug existed in every journey composed through a hub.
- **Integrator:**
  - the roster lists Subrata as scheduled;
  - content pins at 145 files and 181 records;
  - Subrata's spawn hook;
  - a `size()` fix in Subrata's binding test.

**Exact checks:**
- **Rung 2:** at `b96234a`, **Succeeded**.
- **Headless automation:** **100/100 Success** in one run, including:
  - `MiraPresence` 8/8, `NpcRoster`, `SubrataBindings`, `NpcRoute` (with the new hub-spur test), `CreationPause`, `LaneStair`, `Acceptance`, `PersistenceFlow`, `KitDoors`, `KitFrontages`, `ShopFurniture`, `MiraBindings` and the rest.
- **Expected warnings:**
  - Subrata's tall-flats passage legs are 395 and 390 cm, against the schedule's 500 cm minimums, so his walk is paced at about 78–79%;
  - his roster row is not yet marked "placed".
- **Rung 1:** the merged run is still pending.
- **Next:** the packaged build and the first offscreen acceptance spike (AW-01/02).

**Plan:** `Docs/ROUND3_PLAN_2026-10-19.md` (gpt-6.1-sol, NR-01..06).
- **Running:**
  - NR-01: Rupa Ghosh, the row-house resident, and the `doors_knock` scenario (Claude Sonnet);
  - NR-02: Nirmal Bose, tall-flats unit A, and `save_restart` (gpt-6.1-sol);
  - NR-04: person looks, `anindita_mira` and `origins_seating_cameras` (Claude Sonnet).

## Checkpoint — the first automated packaged walk ran offscreen, 2026-10-19

**Package:** Rung 4 at `b96234a`, `BUILD SUCCESSFUL`. Executable SHA-256 `6662afa7…`.
- The first attempt shipped a truncated executable: exactly 2 MiB, against about 335 MB. A power-off at 12:27 had cut the link short, and UBT then treated the file as up to date. Windows refused to launch it ("file is corrupted and unreadable", WinError 1392).
- The truncated file was deleted and relinked. The spike job now deletes the executable before packaging whenever it is under 100 MB.

**Acceptance spike:** `run_lane_acceptance.py --scenario boot_walk_interact --start new`, offscreen, one process. No owner windows were open.
- **Passed:**
  - `world_ready`: offscreen flag on, 1280×720 viewport, 5 seats and 16 doors built;
  - `enter_world`: a new family life;
  - `walk_to_living_doorway`: reached in 1.4 s by real movement input;
  - `shot_living_doorway`: a real rendered PNG, `Docs/Images/acceptance-offscreen-living-doorway.png`, showing the clock "Mon 2 Jun · 07:32" and the "Your life has begun" notice.
- **Failed:** `walk_to_table_chair`, blocked 160 cm short. The straight-line path runs into the open living-room door leaf, so this is a driver path fix, sent back to AW-01's author.
- **Your saves:** a private `-UserDir`, with verified backup and restore of the real save slot. "Save restoration verified: True".
- **Overall:** FAIL (incomplete scenario). **Offscreen capture is proven on this VM.**

## Checkpoint — round 3, batch A: Rupa, Nirmal, person looks and the acceptance scenarios; Rung 1 175/175 and engine 108/108, 2026-10-19

**What merged:**
- **gpt-6.1-sol, NR-02:**
  - Nirmal Bose of tall-flats unit A: off-lane work, home and awake through 17:00–20:30 visiting.
  - His bindings from the unit doorstep.
  - The `save_restart` scenario.
- **Claude Sonnet, NR-01:** Rupa Ghosh of the row house (home inside 16:00–19:30 visiting), her bindings, and the `doors_knock` scenario.
- **Claude Sonnet, NR-04:**
  - Person looks split into `PerfectWorldNpcLooks` (bespoke figures for Mira, Anindita, Subrata, Rupa, Nirmal and Sahana).
  - The `anindita_mira` and `origins_seating_cameras` scenarios.
- **Claude Sonnet, AW-01 path fix:** the chair walk goes around the household adult. Her capsule, not the door leaf, had blocked the spike.
- **Integrator:**
  - spawn and scenario hooks;
  - content pins at 147 files and 183 records;
  - Nirmal's travel legs re-sorted;
  - two compile fixes;
  - roster rows for Nirmal and Rupa;
  - the NpcLooks fix merged (hair stays inside the authored height).

**Problems found along the way:**
- **Stale generated headers:** UHT had not regenerated `*.generated.h` after headers changed, because the clock jumped, so the line-keyed `GENERATED_BODY` no longer matched. Deleting `Intermediate/.../UHT` fixed it.
- **Invalid schedule data:** Nirmal's schedule had unsorted travel legs, written in a sandbox that could not compile. The loader refused the file, which emptied the content database and failed 10 dependent tests.

**Exact checks:**
- **Rung 1:** at `b44782a`, **175/175**.
- **Rung 2:** **Succeeded**.
- **Headless automation:** **108/108 Success** in one run.

## Checkpoint — round 3, batch B merged (not yet built in Unreal): Sahana, three more frontages, the first real upstairs, 2026-10-19

**What merged (`d6f1add`, pushed):**
- **Claude Sonnet, NR-03:** Sahana Mitra of tall-flats unit B.
  - Sewing at home, plus an off-lane errand.
  - Home and awake for every minute of her door's 17:00–20:30 visiting hours.
  - A remembered sewing agreement.
  - Her bindings, and the `resident_homes` scenario covering Subrata, Nirmal, Rupa and Sahana: host truth, real knocks, Talk and Leave, and save read-back.
  - Focused portable tests pass: `SahanaLifeTests`, content, and `pwcontent` at 148 files and 184 records.
- **gpt-6.1-sol, NR-05:** three more catalogue frontages, with 794 focused portable assertions passing:
  - `repair_workshop` (−Y/3, X 800–1700, Workshop);
  - `hardware_store` (+Y/8, X 5200–6400, GeneralGoods);
  - `old_house` (+Y/7, X 4500–5200, Residence).
  - Planned door definitions for a 16→19 door migration. Doorways stay unhung until it lands.
  - The `frontage_three` scenario.
- **gpt-6.1-sol, NR-06:** the first real upstairs.
  - The stair fits inside `tall_flats` without losing either ground unit: 130×640 cm, two 120 cm landings, 17 risers from 15 to 315 cm, a 130×418 slab opening, 220 cm headroom and 100 cm guards.
  - A furnished upper dwelling (its door is planned for a 19→20 migration), its presenter, and the `tall_flats_upstairs` scenario.
  - 10,303 layout assertions and 231,482 stair assertions pass.
- **Integrator hooks:**
  - Sahana's spawn and roster row;
  - three scenario dispatches;
  - the tall flats are built by `BuildTallFlatsUpstairs` before the generic kit branch;
  - CMake lines;
  - content pins at 148 files and 184 records.

**Not run** (the owner asked to hold builds): the merged Rung 1, the Unreal compile, automation, and packaging. The parked package and acceptance job (`D:\PWWork\job_acceptance.ps1`) is ready to rerun.

**Remaining sealed:** 9 side frontages and the 2 end masses.

## Checkpoint — doors 16→20 merged; round 4 under way, 2026-10-19

**What merged (`843f73e`):** the door roster grows from 16 to 20 doors (Claude deep-work agent).
- New doors: the repair workshop and hardware store (business hours), the old house (private visiting), and the tall-flats upper flat (private).
- 16-door saves migrate and authenticate against their stored door text.
- A real frozen 16-door fixture.
- The three frontages now hang their doors.
- The hash goldens were re-pinned.
- **Rung 1:** 178/178 on the branch.
- **Unreal:** not built, by the owner's request to hold builds.
- **Follow-ups:**
  - The old house and the upper flat need a resident or a real invitation before the walk scenarios can enter them.
  - The upper-flat leaf is authored but not hung yet.

**Round 4** (`Docs/ROUND4_PLAN_2026-10-19.md`, gpt-6-astra):
- **R4-03, gpt-6.1-sol:** ambient life (vendor Purnima Das with a cart, passers-by Arun Paul and Rekha Bose). Pushed on `content/r403-ambient`, not yet merged: its portable tests could not run in the sandbox.
- **R4-02, Claude Sonnet:** the proprietors Debashis Pal and Mahua Saha.
- **R4-06, Claude Sonnet:** looks and heights for the five newcomers.
- R4-02 and R4-06 are in progress, with WIP pushed on their branches.
- **Queued:**
  - R4-01: three more frontages and a 20→23 door migration;
  - R4-04: journal state;
  - R4-05: the "Today" journal screen.

## Checkpoint — R4-02 and R4-03 merged; Rung 1 181/181, 2026-10-19

**Merged:**
- **R4-03** (`f49c445`, gpt-6.1-sol): vendor Purnima Das with her block cart, and the passers-by Arun Paul and Rekha Bose.
- **R4-02** (`81d5541`, Claude Sonnet): the proprietors Debashis Pal (repair workshop, 171 cm) and Mahua Saha (hardware store, 162 cm).
  - Their schedules, conversations, bindings and the `proprietors_workday` scenario.
  - The schedule and catalogue tables now hold 12 entries each.
  - The roster rows are not yet marked placed.

**Rung 1 on the merged tree** (fresh build directory `D:/pwm81d`): **181/181 passed**. `pwcontent validate` passes with 153 files and 189 records, matching the engine pins.

**Not run** (the owner is holding builds): the Unreal compile, automation, packaging and the AW scenarios.

**R4-01 in progress** on `core/r401-frontages`:
- **Core** (gpt-6.1-sol): three GeneralGoods frontages (`market_stalls_a`, `market_stalls_b`, `end_shop`) and the 20→23 door edition.
- **Fixture:** the frozen 20-door fixture was generated by the unchanged generator from `git archive 843f73e`.
- **Goldens:** re-pinned after the hash test agreed with the reference encoder.
- **Unreal half** (Claude Sonnet, not compiled):
  - stall faces brought forward of the kit wall, with a 120 cm route kept;
  - posted-hours boards derived from the door definitions;
  - "Browse goods" interactables showing "Unstaffed. Purchasing unavailable.";
  - `frontages_round4` registered for the restart pair.

**R4-04** (journal state, gpt-6.1-sol) is running on `core/r404-journal`.

## Checkpoint — R4-01 and R4-04 merged; Rung 1 183/183; Round 5 planned, 2026-10-19

**R4-01** (`39912f6`):
- Three browsing frontages (`market_stalls_a`, `market_stalls_b`, `end_shop`) and the 20→23 door edition.
- The real frozen 20-door fixture, generated from `git archive 843f73e`.
- Unreal production faces (Claude Sonnet): stalls, posted hours, "Browse goods", and `frontages_round4`.
- Rung 1: 182/182 on the branch.

**R4-04** (`7f3c045`, gpt-6.1-sol; integration fixes by Claude):
- The lane journal: dated encounters per person per day, and agreements derived from conversation facts.
- Save schema 9, hash version 6, and `pw.lane_journal/1`. Older saves keep their agreements with an empty encounter log.
- The journal test had assumed the rota conversation was open at breakfast; it now moves to the real conversation windows. The rollback test now asserts that only the event cap refuses its paid choice.
- Current-schema assertions read `WorldSaveSchemaVersion`.
- Goldens re-pinned after the reference encoder agreed.
- `journal_state` is registered, including as a restart pair.
- **Rung 1 on the merged tree: 183/183.**

**Not run:** the Unreal compile, automation, packaging and every AW scenario. The owner is holding builds.

**Round 5** (`Docs/ROUND5_PLAN_2026-10-19.md`, gpt-6-astra) recommends serial engine verification before merging more Unreal features. That decision is put to the owner. Branch work continues.

**R4-05** ("Today" journal screen, Claude Sonnet) is in progress on `ui/r405-today`.

## Checkpoint — v0.2.0-alpha published, 2026-10-19

**Released as `v0.2.0-alpha` (prerelease):**
- Tag `v0.2.0-alpha` on `0c32eea`: https://github.com/debdutta777/Perfect-World/releases/tag/v0.2.0-alpha
- Asset `PerfectWorld-v0.2.0-alpha-Win64.zip`: 410,987,006 bytes, sha256 `feaeb77d9293ebdea9b133074487044bd0ad6d590d362487477e04f1c5d59534`, 201 files. It is the Development packaged build, with the `Saved` folder left out.

**First compile of the Round 3/4 backlog.** The game target failed in two places, both now fixed:
- `PerfectWorldAcceptanceHomes.h` forward-declared `NpcSchedule` as a struct, but it is a type alias. It now includes `NpcScheduleContent.h`.
  - The scenario also called the private `IsHostHome`. It now uses the public `IsHostHomeForAcceptance`.
- `PerfectWorldAcceptanceTests.cpp` reached `PerfectWorld::ParseJson` only through unity-blob neighbours: a bare `"Json.h"` can be the engine's. It now includes `ContentRegistry.h`.

**Verification:**
- **Rung 1:** 183/183.
- **Game target:** compiled.
- **Rung 4:** package succeeded (BuildCookRun exit 0; exe 337,518,080 bytes).
- **Not run:** the editor build (Rung 2), engine automation, and every AW scenario. It is not boot-tested, because the owner's windows were open, so the game was not launched.

**Next:** Round 5. R5-01, R5-04 and R5-06 are on their branches, awaiting integration.

## Checkpoint — v0.3.0-alpha published; owner feedback, Rounds 6 and 7 under way, 2026-10-20

**Released as `v0.3.0-alpha` (prerelease):**
- Tag on `7b553a3`: https://github.com/debdutta777/Perfect-World/releases/tag/v0.3.0-alpha
- Asset `PerfectWorld-v0.3.0-alpha-Win64.zip`: 411,785,449 bytes, sha256 `0780e295e348309d34cc97aec02b9e31887e22410a51adbd91f3cc086a171403`.
- **Verification:**
  - Rung 1: 187/187.
  - Game target and editor target compiled.
  - Package: BuildCookRun exit 0. A host power-off killed the first cook, and the second run succeeded.
  - Automation, acceptance and a boot test were not run.

**Owner feedback after playing:**
- a visual pentagon for creation points;
- a minimap and a full map;
- a quest window;
- key mapping in Settings;
- ring a calling bell before entering homes;
- the city should look like real Kolkata, with far more people (students), shops, shopkeepers, a proper school and roads.
- **Orders (Claude Sonnet, branches `ui/f1-pentagon`, `ui/f2-map`, `ui/f3-quests`, `ui/f4-keybinds`, `game/f5-doorbell`):** F1 pentagon, F2 map, F3 quests, F4 keybinds, F5 doorbell.
- **The city:** `Docs/ROUND7_PLAN_2026-10-20.md` (gpt-6-astra). R7-01, the portable spatial contract, is running (gpt-6.1-sol).

**Round 6:**
- R6-01 (last three frontages, 29 doors) and R6-02 (slot summary) are implemented and being integrated.
- R6-03 (scenario repairs) and R5-05 (working-proprietor bridge) are on their branches, waiting for an Unreal compile.
- R6-06 (tailor) and R6-07 (school arrival) stopped when the ChatGPT account reached its usage limit. Their partial work is kept, and they will be relaunched once.

## Checkpoint — v0.4.0-alpha published (owner feedback), 2026-10-21

**Released as `v0.4.0-alpha` (prerelease):**
- Tag on `cafc35e`: https://github.com/debdutta777/Perfect-World/releases/tag/v0.4.0-alpha
- Asset: 413,924,210 bytes, sha256 `781a9d068f01b299d3942df1570cb7f7b106c57fcacc8e093b8f977643ffac5c`.

**Owner feedback items merged (Claude Sonnet):**
- **F1:** a seven-axis trait radar. The core has 7 trait domains, so the chart is not a pentagon.
- **F2:** a portable lane map projection, an upper-right minimap, and a full world map opened with M or D-pad Left.
- **F3:**
  - a portable quest projection;
  - a quest window opened with J or D-pad Right, or from the pause menu;
  - a tracked-objective line.
- **F4:** key remapping (a binding store, and a Controls tab in Settings), with Settings now in the pause menu.
- **F5:** calling bells at every dwelling entrance; a threshold guard against un-admitted entry through open leaves; the household closes a visitor's door; knocks reset on load; family start shells count as home.
- **Fixes during integration:**
  - four first-compile errors;
  - the bell roster grew to include R6-01's two residences;
  - the lane map knows R6-01's three buildings;
  - the school-day test names content files as `/relative` paths.

**Also merged:**
- R5-05: the working-proprietor bridge.
- R6-01: the last three side frontages, 29 doors, a frozen 26-door fixture, goldens re-pinned.
- R6-02: the slot summary.
- R6-03: scenario repairs.
- R6-07: school arrival through the reducer.

**Verification:**
- Rung 1: 194/194.
- The editor and game targets compiled.
- Package exit 0.
- Not run: automation, acceptance, and a boot test.

**Next:**
- **R6-06**, the staffed tailor Satyajit Dutta: being integrated.
- **R7-01**, the neighbourhood spatial contract: on its branch.
- **Round 7 in flight:**
  - R7-05 crowds and R7-02 traffic: the portable models, by gpt-6.1-sol.
  - R7-02 streets and R7-03 school campus: Unreal, by Claude Sonnet.

## Checkpoint — v0.5.0-alpha published: Kolkata streets and Nabapara Vidyalaya, 2026-10-22

**Released as `v0.5.0-alpha` (prerelease):**
- Tag on `b62900d`: https://github.com/debdutta777/Perfect-World/releases/tag/v0.5.0-alpha
- Asset: 414,793,414 bytes, sha256 `b75c98fb9403596b107d8f8a3378fb7bc0f308e80a28a855d76cc9ccee6d8299`.

**Merged:**
- **R6-06 (gpt-6.1-sol), the tailor Satyajit Dutta.** Integration added the registrations, content pins at 156 files and 192 records, and readable Today text for Prabir's, Ishita's and the tailor's agreements.
- **R7-01 (gpt-6.1-sol), the neighbourhood spatial contract.** Integration fixed one thing: R6-01's three opened buildings now come from the kit catalogue rather than a duplicate lot.
- **R7-02 streets (Claude Sonnet).** The north alley reopened as the cross street, and the old +Y asphalt box was retired.
- **R7-03 campus (Claude Sonnet).**

**Verification:**
- Rung 1: 196/196.
- The editor target compiled; the about 40 new files compiled clean on the first attempt.
- Package exit 0, with 0 cook errors.
- Not run: automation, acceptance, and a boot test.

**Known follow-ups:**
- The crowd model's student windows must follow `bell_schedule.nabapara_xi_science` (gates 09:30, first bell 10:00, dismissal 15:50), not the plan's 06:45–07:30.
- Gate sign-in copies the world; it should move to a lane-life command path.

**Environment:** the host now powers off about every 20 minutes, and three GPT jobs were killed twice.

**In flight (gpt-6.1-sol):**
- R7-05 crowd presenter;
- R7-02 traffic presenter;
- R7-04 staffed shops;
- R8-01 commerce contract (`Docs/ROUND8_PLAN_2026-10-22.md`).

## Checkpoint — crowds and traffic merged; staffed shops on their branch, 2026-10-23

**Merged at `835ee1b`:**
- **R7-02 traffic presenter:** gpt-6.1-sol, finished by Claude Sonnet after the ChatGPT usage limit.
- **R7-05 crowd presenter:** the same.
  - Student windows now follow `bell_schedule.nabapara_xi_science`: arrive 09:15–10:00, leave 15:50–16:30.
- **Hooks:** `SpawnTrafficPresenter`, `PWCrowd::SpawnCrowdPresenter`, `traffic_round7`, `crowd_round7`.
- **First-compile fixes:** player pointers were `TObjectPtr` read through `auto*`, and `FMath` rounding returned int64 for the portable rectangles.

**Verification:**
- Rung 1: 198/198 on the combined crowd and traffic tree.
- The editor target compiled.
- Not run: packaging, automation, acceptance, and a boot test.

**R7-04 staffed shops (`content/r704-shops`, not merged):**
- Bimal Modak and Tithi Ghoshal at the sweet shop; Aparna Lahiri at the pharmacy; Gouranga Nandi at the tea stall; Koushik Mondal selling jhalmuri.
- Rung 1: 197/197 on an integrated scratch copy.
- **Blocked on the owner:** the streets' counters are solid across each keeper's axis. A 120 cm service-flap patch (`D:\PWWork\r74_shoplots.diff`, `r74_stalls.diff`) was refused to the agent by the permission system and awaits the owner's decision.

**R8-01 (commerce contract)** waits for the ChatGPT usage reset.

## Checkpoint — v0.6.0-alpha published (crowds and traffic); R8-01 merged, 2026-10-23

**Released as `v0.6.0-alpha` (prerelease):**
- Tag on `25a1931`: https://github.com/debdutta777/Perfect-World/releases/tag/v0.6.0-alpha
- Asset: 415,549,584 bytes, sha256 `789d2f7cb75bc6e01fa79279de4a2561b26fc022a1fab30952375947dc962452`.
- **Verification:**
  - Rung 1: 198/198.
  - The editor and game targets compiled.
  - Package exit 0.
  - Not run: automation, acceptance, and a boot test.

**R8-01 (gpt-6.1-sol), the portable commerce contract, is merged:**
- a wallet in paise;
- a household-rank allowance;
- inventory;
- quotes and purchase refusals;
- atomic purchases with receipts;
- consumption.
- Rung 1: 199/199. No save-format change.

**In flight:**
- R8-02 content and R8-03 reducer integration (gpt-6.1-sol).
- R8-05 Buy panel, wallet and inventory UI (Claude Sonnet).
- R8-04, the save migration to schema 10 and hash 7, follows R8-03 (Claude deep-work).
- The staffed shops (R7-04) still wait on the owner's decision about the counter patch.
