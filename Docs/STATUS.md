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
