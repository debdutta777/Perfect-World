# Development workflow

**Document status:** authoritative on process, 2026-09-23. `GAME_PLAN.md` is the single entry point and wins on any conflict; this document is what §9 of that plan points to. It governs how the owner and AI agents turn `ROADMAP.md` steps into verified, committed work. It does not define the game — see `GAME_PLAN.md`, `SYSTEMS.md`, `ASSET_PIPELINE.md`, `ROADMAP.md` for that.

This is a solo project: one owner, several AI agents (Claude sessions and subagents, Codex/GPT), no other humans. Every rule below exists to keep that combination honest — to stop an agent from claiming more than it verified, from colliding with another agent's build or files, or from making a decision that belongs to the owner.

---

## 1. Roles

**The owner decides, plays and judges.** Concretely: every design decision that changes what the game is (scope, tone, cultural content, new mechanics not already specified in a `Docs/*.md`); playtesting and judging whether movement, camera, combat timing or pacing *feel* right; final say on art direction once M9 begins; cultural review of anything Kolkata/West Bengal-specific (`INDIA_WORLD_BIBLE.md` is authoritative there, but the owner signs off); and resolving any open question in `GAME_PLAN.md` §12.

**AI agents implement, test, tool and document.** Concretely: portable C++ reducers, state machines, serializers, migrations, validators, deterministic RNG, schedule resolution; unit/property/fuzz/migration/replay tests; narrow Unreal bridges, input plumbing, build scripts, debug overlays; schema generation and stable-ID audits; greybox helper code; content authoring from an existing template (a new problem family, event or nation that fits an established pattern); first drafts of dialogue and schedule tables for the owner to edit. This list is `MASTER_PLAN.md` §5 ("well suited"), still binding.

**What agents must not do**, per `MASTER_PLAN.md` §5: judge whether something feels good; finalize modelling, rigging, animation, lighting, VFX, sound or music; maintain large binary Blueprint/level graphs unattended; assert authentic contemporary Kolkata/Bengali detail without the owner's cultural review; decide what should *not* be built. Voxel-block placeholders are not exempt from art judgment forever — M9 real-art integration is still an owner-led visual decision, even though it is agent-implemented.

**Escalation rule.** If an agent, mid-step, hits a fork that is a real design decision — not "which valid implementation" but "what should the game do" — it stops implementing that fork, records the question plainly (in `STATUS.md` under the step, and if it is scope-level, as a new row in `GAME_PLAN.md` §12), and either asks the owner directly or proceeds only on the narrowest interpretation that keeps existing behaviour unchanged, flagging the assumption. It never silently picks the design answer it finds most elegant. An agent may make ordinary implementation judgment calls (naming, file layout, which existing pattern to follow) without escalating.

---

## 2. The step loop

Every unit of work follows this loop, no exceptions:

1. **Pick one step from `ROADMAP.md`.** Not a milestone, not a vague direction — one bounded, verifiable step. If the step as written is too large to state one pass condition for, split it in `ROADMAP.md` first (that split is itself worth recording).
2. **State intent and pass condition before writing code.** One sentence each. If you cannot state the pass condition, the step is not ready to start.
3. **Implement.** Touch only the files the step requires. Follow existing patterns (naming, error handling, test structure) rather than introducing a new one mid-project.
4. **Run the verification rung the step's kind requires** (§3). Not a lower rung, not "should pass" — actually run it.
5. **Record the result in `STATUS.md`**, using the format below.
6. **Commit** (§7), with the check's actual result stated honestly in the body, including what the rung does *not* prove.

### Record format

Every `STATUS.md` entry for a step carries these six fields, in this order:

| Field | Content |
|---|---|
| **Intent** | The one sentence from step 2, verbatim or close to it. |
| **Files changed** | Exact paths. If a file was added, say so; if generated files (UHT, Intermediate) changed, they are not listed — see `.gitignore`. |
| **Exact check** | The literal command run (see §3) and, for a manual rung, what was clicked/observed. |
| **Result** | PASS/FAIL/NOT RUN per rung, stated as plainly as `STATUS.md` already does elsewhere (`"NOT RUN"` is a valid, honest result — better than an inflated one). |
| **Remaining limitation** | What this step does *not* cover yet — the thing a reader would wrongly assume is done if this line were omitted. |
| **Next smallest action** | The next step this one unblocks, one sentence. |

**Worked example**, using the already-completed clock step (commit `00e5425`) as the template:

> **Intent:** Add one monotonic game-minute clock with deterministic scheduled-event firing and save round trip.
> **Files changed:** `Source/PerfectWorldCore/Public/GameClock.h`, `Source/PerfectWorldCore/Private/GameClock.cpp`, `CMakeLists.txt`, `Tests/GameClockTests.cpp`.
> **Exact check:** `python Scripts/build_core.py --build-dir C:/Users/DEBDUTTA/AppData/Local/Temp/pwcore` — both `PerfectWorldCoreTests` and `PerfectWorldClockTests` via CTest.
> **Result:** PASS — 2/2 test executables, 16,857 new clock assertions (calendar round trips, leap/century years, weekday advance, firing order, exactly-once delivery, determinism, monotonicity). `PerfectWorldEditor` compiles.
> **Remaining limitation:** Not exercised in a launched window; no Unreal-facing subsystem yet; no gameplay consumes the clock.
> **Next smallest action:** Expose the clock to Blueprint through a subsystem (§8 recipe), or wire condition meters (Immediate Build Order step C) against it.

A step that skips a field, or that writes "done" without the check, is not recorded correctly — fix the `STATUS.md` entry before moving on.

---

## 3. The verification ladder

A step is **not done** until the rung its kind requires has actually been run. Never write or imply a higher rung than the one that ran. Each rung proves a specific thing and nothing past it.

### Rung 1 — CTest (portable rules)

```
python Scripts/build_core.py --build-dir C:/Users/DEBDUTTA/AppData/Local/Temp/pwcore
```

Configures CMake (Visual Studio 17 2022, x64), builds Release, runs CTest for **both** `PerfectWorldCoreTests` and `PerfectWorldClockTests`. Both must pass — one green executable is not the suite.

- **Proves:** the portable C++20 logic (Source/PerfectWorldCore) is correct against the assertions written, including edge cases, migrations and fuzzed sequences the tests cover.
- **Does not prove:** that Unreal compiles against this code, that the Blueprint-facing subsystem exposes it correctly, that anything is visible or playable, or that untested edge cases behave correctly.

### Rung 2 — Editor build (UHT + compile)

```
python Scripts/build_unreal.py
```

Runs `Build.bat PerfectWorldEditor Win64 Development` with `-NoUBA`. **Close the Unreal Editor first** — it locks the module DLLs and the build will fail or corrupt the lock.

- **Proves:** Unreal Header Tool generates reflection code without error, the portable module links as a DLL, and the Unreal-facing code (subsystems, actors, Slate/UMG) compiles against the current engine.
- **Does not prove:** that anything behaves correctly at runtime, that Blueprint graphs wired to the new code work, or that the change is visible or usable.

### Rung 3 — Play In Editor (PIE) smoke — visible behaviour

Manual. Open the project (`"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/Users/DEBDUTTA/Desktop/PerfectWorld/PerfectWorld.uproject"`), Play In Editor with input focus in the viewport, and exercise the specific behaviour the step claims. The old menu smoke checklist (in git history before ROADMAP M0-13a) is the model for how to write this down: numbered, specific, falsifiable steps, not "looks fine."

This rung needs the owner, or a documented manual script an agent can execute against a description of expected results and report faithfully (an agent cannot judge "does this feel right," but it can confirm "does clicking X produce Y").

- **Proves:** the described interaction is visible and behaves as stated, in this one launched window, on this one machine.
- **Does not prove:** it works cooked, on another machine, under load, with controller/touch input unless that was specifically exercised, or that it *feels* right — that judgment is the owner's alone.

### Rung 4 — Cooked Windows build — milestones

A packaged, standalone build, reserved for milestone gates (`GAME_PLAN.md` §8) — not run per step. Close the editor first, and never run it alongside another Unreal build:

```
python Scripts/package_windows.py --engine-root "D:\Epic Games\UE_5.8" --output D:\PWBuilds
```

This runs RunUAT BuildCookRun (Development, `-NoUBA`) and writes `D:\PWBuilds\Windows\PerfectWorld.exe`. Then boot it with `-windowed` and read `D:\PWBuilds\Windows\PerfectWorld\Saved\Logs\PerfectWorld.log`. Expected: `Game class is 'PerfectWorldBlockTestGameMode'`, no `Error:` or ensure, and a clean `LogExit: Exiting.` after closing. The first cook of a fresh checkout takes several minutes.

- **Proves:** the game packages and runs outside the editor.
- **Does not prove:** Android or other platforms (a named-device test is separately required, per `MASTER_PLAN.md` §5), or that a milestone's actual playable/gate condition is met — check the milestone's stated gate separately.

**Rule:** a portable-only change stops at Rung 1. A change that touches the Unreal module needs Rung 2. A change that claims visible or interactive behaviour needs Rung 3 — code that compiles but was never played is not "done," it is "compiles." Milestone gates need Rung 4 plus their own stated condition (e.g., M4's blind-test gate is not a build rung at all — it is a separate, owner-run playtest).

---

## 4. Environment gotchas

Facts that cost real time to discover once. Do not rediscover them.

- **Bash heredocs corrupt escapes.** In this agent environment, `cat <<EOF`, `sed`, and Python heredocs silently collapse `\\` to `\`. This corrupts any C++ string literal with an escape and any JSON. **Always write or edit C++ and any file containing backslash escapes with the Write/Edit tools**, never by piping a heredoc through the shell.
- **MSBuild FileTracker breaks on long build paths.** Build the portable core with a short directory: `python Scripts/build_core.py --build-dir C:/Users/DEBDUTTA/AppData/Local/Temp/pwcore`. The `MSB8029` "intermediate directory under Temp" warning that results is harmless — ignore it, do not try to relocate the build dir back under the repo.
- **Two test executables, both required.** The portable suite is `PerfectWorldCoreTests` and `PerfectWorldClockTests`. CTest reports them separately; a step is not verified at Rung 1 unless both pass.
- **The Unreal Editor must be closed to rebuild.** `python Scripts/build_unreal.py` builds `PerfectWorldEditor Win64 Development` with `-NoUBA`. The running editor locks the module DLLs. **Never run two Unreal builds at once** — a second `build_unreal.py` invocation while one is in flight will corrupt or fail both.
- **Launch path:** `"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/Users/DEBDUTTA/Desktop/PerfectWorld/PerfectWorld.uproject"`.
- **Unreal MCP exists only while the editor runs.** The editor starts Epic's MCP server at `http://127.0.0.1:8000/mcp` about 35 seconds after launch (`Config/DefaultEditorPerProjectUserSettings.ini`). It has no authentication and binds to localhost; never expose the port. A Claude Code session that started while the editor was closed has no Unreal tools: run `/mcp` and reconnect `unreal`, or start a new session. Tool search is on, so the server exposes three meta-tools (`list_toolsets`, `describe_toolset`, `call_tool`) and the toolsets are discovered through them. The plugins are enabled for the Editor target only, so they never ship in a game build.
- **Unreal's macros leak into the portable core.** The core also compiles as an Unreal module, where `check`, `verify`, `ensure`, `checkf` and similar are assertion macros. A portable identifier with one of those names (for example a callback parameter called `check`) passes Rung 1 and then fails Rung 2 with a confusing error such as `binary '!': no operator found`. Never use those names in `Source/PerfectWorldCore`.
- **A Rung 2 compile can look stalled when D: is throttled.** `PerfectWorldCoreParityTests.cpp` is the only game-module file that uses Unreal's shared precompiled header (`SharedPCH.UnrealEd...pch`, about 2.4 GB). `cl.exe` memory-maps it, so it shows almost no CPU and no counted reads while it waits. On 2026-10-11 the cloud PC's D: read at under 1 MB/s with about 0.3 s per read while agents' CMake builds also ran, which makes that one compile take many minutes. It is not a hang. A rerun is faster only because the file is partly cached by then. Check `\PhysicalDisk(1 d:)\Disk Read Bytes/sec`, avoid running agent builds during Rung 2, and wait rather than kill. **The fix that works:** read the PCH once sequentially, with large buffers, so it sits in the file cache. Memory-mapped page faults are tiny random reads (under 1 MB/s at about 0.3–0.6 s each on the throttled disk). A sequential read ran at 17 MB/s (2.4 GB in 139 s), after which the stalled compiles moved at once. For example, in PowerShell: `$f=[IO.File]::Open($pch,'Open','Read','ReadWrite'); $b=New-Object byte[] 8MB; while($f.Read($b,0,$b.Length)){}; $f.Close()`.
- **Picking the compiler.** 5.8 bans some MSVC versions in `Engine/Config/Windows/Windows_SDK.json`. Unreal's build tool judges the compiler inside a toolset folder, not the folder name: the `14.50.35717` folder here holds compiler 14.50.35739, which is allowed and preferred.
- **Codex CLI needs stdin, not an argument.** `codex exec` must receive its prompt on stdin: `cat prompt.txt | codex exec -m gpt-6-astra --sandbox read-only`. Passing a large prompt as a command-line argument in a background job leaves it waiting on stdin and it exits with nothing — always pipe the prompt in.
- **Codex model IDs:** `gpt-6-astra` and `gpt-5.6-sol` work. Plain `gpt-6` is refused on this ChatGPT account — do not retry it expecting a different result.
- **Codex has a usage limit.** When hit, preserve whatever partial output exists, report the limit honestly in `STATUS.md`, and do not retry in a loop hoping it clears. A different model is not a workaround for a shared account limit — do not claim it is.
- **Unreal regenerates config files on its own.** `Config/DefaultEngine.ini` (the Android file server section) and `Config/DefaultInput.ini` churn on editor launch. Leave that churn uncommitted (`git status` will show it as modified but unstaged is fine to leave, or discard with `git checkout -- <file>` if it is noise) unless the change was actually intentional — check the diff before committing either file.
- **`Content/Fab/` is gitignored.** Downloaded/imported art kits stay local, never committed. Git LFS is configured (`.gitattributes`) for `.uasset`, `.umap`, `.fbx`, `.tga`, `.exr`, `.wav` — real binary art added outside `Content/Fab/` goes through LFS automatically; verify with `git lfs status` before a large commit.
- **No `Co-Authored-By` trailer on commits.** The owner's global `CLAUDE.md` forbids it unless this project's `.claude/settings.json` sets `attribution.commit` — it does not. The Bash tool's default commit template may suggest one; ignore it. (Commits already on `main`, e.g. `00e5425`, `4e6a8d8`, `c356846`, carry `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>` from before this rule was written down here — do not repeat that pattern, and do not rewrite that already-pushed history without the owner asking.)
- **Pushing to `origin main`** (`github.com/debdutta777/Perfect-World`, private) happens only when the owner asks, or has already authorised it for the current session. Committing locally does not imply permission to push.

---

## 5. Model routing

| Model | Role | Invoked via |
|---|---|---|
| **Claude (this CLI, Sonnet by default)** | Integrates and implements in the repository: writes and edits code, runs builds/tests, commits, is the session that owns the working tree at any moment. | Claude Code CLI in the repo |
| **GPT-6 (`gpt-6-astra`)** | Design critique and second opinions — not implementation. Used when a design document or plan benefits from an outside read before the owner commits to it. | Codex CLI: `cat prompt.txt \| codex exec -m gpt-6-astra --sandbox read-only` |
| **GPT-5.6 Sol** | Routine implementation when used — ordinary C++, tests, data authoring, mechanical edits. Not the default execution path for this CLI session, but may be dispatched via Codex for parallel bounded work. | Codex CLI, same stdin-piping rule, `-m gpt-5.6-sol` |
| **Claude Opus (subagent)** | Reserved for genuinely difficult work: architecture, concurrency, tricky migrations, security review, stubborn debugging. Not the default — do not silently escalate routine work to Opus. | `Agent` tool, `model: "opus"` |
| **Claude Sonnet (subagent)** | General implementation and research subagents for bounded, parallelizable pieces of a step. | `Agent` tool, default model |

**Invoking Codex correctly:** always pipe the prompt on stdin (`cat file | codex exec ...`), never pass it as a trailing argument, and never run it in the background with a large prompt as an argument — it will hang waiting on stdin and return nothing. Use `--sandbox read-only` for critique/review tasks that should not touch the working tree; only grant write access when the task is explicitly implementation and the file ownership rule in §6 is respected.

**Quota handling:** on a Codex usage-limit error, stop. Save whatever partial output was produced, write it into the step's `STATUS.md` entry as-is (marked partial), and report the limit. Do not loop retries, and do not treat a different model ID as a bypass for a shared account limit — `STATUS.md`'s own execution record already states this plainly and that discipline continues here.

**Verify before accepting.** Every claim a model makes about what it did — "tests pass," "the build succeeds," "this file was updated" — is checked against the actual code and the actual rung result before it is written into `STATUS.md` as fact. This is true across models, Claude included: a model's self-report is a claim, not a verification. The integrating Claude session re-runs or directly inspects the evidence (diff, test output, build log) before recording a result. `STATUS.md`'s existing "Model execution record" section (the Opus-review-blocked entry, the "no claim is made that model switching avoids shared account limits" line) is the standard to keep matching.

---

## 6. Multi-agent rules

- **File ownership per task.** Before spawning or dispatching parallel agents (subagents or Codex), split the step's file list so no two agents touch the same file in the same window. State the file list in each agent's prompt.
- **No two agents editing the same file concurrently**, even if their changes seem non-overlapping — merge conflicts in generated C++/Unreal files are expensive to untangle by hand. If two pieces of work need the same file, sequence them instead of parallelizing.
- **No parallel Unreal builds, ever.** Only one `build_unreal.py` invocation at a time, from any agent, anywhere. Check for a running build before starting one.
- **The editor must be closed during any Unreal build.** An agent running `build_unreal.py` confirms the editor is not open first; an agent about to open the editor confirms no build is in flight.
- **Agents implement; they do not commit.** The single integrating session — the Claude CLI session actually driving the repo, per §5 — reviews and commits. A subagent or Codex dispatch reports its result back (diff, test output, files touched); it does not run `git commit` itself.
- **Review before commit:** the integrating session reads the actual diff (not just the subagent's summary of it), confirms the stated verification rung actually ran and passed, and only then stages and commits. A subagent's "done" is a claim (§5) until the integrating session has looked at the diff and the test output itself.

---

## 7. Git and commit rules

**Branching:** `main` is fine for this solo project under normal step-by-step work — there is no second developer to protect from in-flight breakage, and `STATUS.md` already carries the "verified / not yet verified" state honestly. Branch when a change is large enough to leave `main` red for more than one step (a schema bump touching several files, a risky architectural change from `MASTER_PLAN.md` §4, or any multi-step piece of work you might want to abandon wholesale) — merge back once its own rung passes on the branch.

**Commit message style**, matching the existing history (`git log`): a short, descriptive subject in sentence case, no ticket-number or conventional-commit prefix, followed by a blank line and a body that explains **why**, not just what changed, and **states verification honestly** — which rung ran, what passed, what remains unverified. See commit `00e5425` for the shape: subject states the deliverable, body explains the design reasoning, closes with an explicit "Verified:" statement naming the exact check and its result.

**No `Co-Authored-By` trailer**, per §4 and the owner's `CLAUDE.md` — this reverses the pattern in commits before this document existed; do not continue it.

**Never committed:** anything under `.gitignore` (`Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`, `.vs/`, `build/`, `*.sln`, `__pycache__/`, `.env*`, `Content/Fab/`); secrets or credentials of any kind; unintentional `DefaultEngine.ini`/`DefaultInput.ini` churn (check the diff first); anything the step's stated rung did not actually verify, described as if it had.

**When to push:** only when the owner asks, or has already authorised pushing for the current session. Commit locally as each step completes regardless — local commits are cheap checkpoints; pushing is a separate, owner-gated action.

---

## 8. Recipes

Each recipe assumes the step loop (§2) and stops at the rung its kind requires (§3).

**Add a problem family.** Author it as data against `SYSTEMS.md`'s problem-family schema: one interaction structure (construct/model/investigate/coordinate), one domain, the authored problem content. Register it in the content registry, run the content validator, and add or extend a CTest that exercises it if any portable scoring/resolution logic is new. No engine code change unless the interaction structure itself is new.

**Add a career/path.** Add the path's ladder data per `PATHS_AND_PROGRESSION.md`'s structure; wire its contribution into the Country Level composite (each of the ten world-summit paths is exactly 10%, per `GAME_PLAN.md` §2); author or reference the problem families the career is built from; add CTest coverage for any new portable rule (progression thresholds, ranking contribution). Ordinary (non-world-summit) careers follow the lighter `ALTERNATE_WORLD.md` career pattern instead.

**Add an authored world event.** Define it as data: trigger condition, the GameClock schedule it fires against, its effect, and its news/route-consequence presentation, per `GLOBAL_WORLD.md`/`ALTERNATE_WORLD.md`. Register and validate. Add a CTest that schedules it and asserts it fires exactly once at the right instant (reuse the clock's exactly-once-delivery pattern from `GameClockTests.cpp`).

**Add a nation.** The roster of fifty in `GLOBAL_WORLD.md` §8 is an owner decision, so a nation outside it needs the owner first. Add its record per `GLOBAL_WORLD.md`'s nation schema (Country Level composite, the ten paths' contributions, its seasonal-update fields). Validate against the content validator; add/extend CTest for any new aggregation rule (e.g., a new path affecting the composite formula).

**Add an asset ID with its block placeholder.** Register the stable asset ID in the asset registry (never reference a file path from gameplay code — `GAME_PLAN.md` §2). Generate its block placeholder from data per `ASSET_PIPELINE.md`'s style guide and generator; confirm the asset resolver returns the placeholder for that ID; add it to the content validator's coverage check so a missing placeholder is caught, not silently absent.

**Integrate a real asset.** Full procedure is `ASSET_PIPELINE.md` — use one of its two import routes (drop folder or editor import), keep the asset ID stable across the swap from placeholder to real art, and confirm the swap changes nothing gameplay-visible (collision, sockets, interaction points stay defined in data against the ID, not the mesh, per `GAME_PLAN.md` §3 rule 3). This is M9-scoped work; do not do it earlier than the milestone calls for.

**Bump the save schema.** Follow the exact pattern in `Source/PerfectWorldCore/Private/CharacterCreation.cpp` (`RestoreSave`) and `CreationSaveSerialization.cpp`, which has gone v1→v4 already:
1. Increment `SaveSchemaVersion` in `CharacterCreation.h` and extend its doc comment to say what each prior version added (the existing comment lists all four — follow that shape).
2. Add the new field(s) to the save struct and to `SerializeCreationSave`/`DeserializeCreationSave`.
3. In `RestoreSave`, add a migration branch: `if (save.SchemaVersion >= N) { ... } ` with an explicit, documented default for older records — never guess-reinterpret old data, migrate it to a stated default (see how version-1/2 cosmetics are discarded in favour of `AppearanceRecord::Default()`, and how an unknown ID at the boundary version is refused outright rather than guessed).
4. Only raise `MinSupportedSaveVersion` if deliberately dropping support for an old save format — that is an owner decision, not a default part of a schema bump.
5. Write CTest coverage: round trip at the new version; migration from every still-supported older version; rejection of an unknown ID/enum introduced at the new version's boundary; rejection below `MinSupportedSaveVersion` and above `SaveSchemaVersion`. Mirror the existing test file's structure (`Tests/CharacterCreationTests.cpp`).
6. Rung 1 (CTest) is the required check; Rung 2 only if the Unreal-facing subsystem struct/enum also changed.

**Add a new portable system with tests.** Create `Source/PerfectWorldCore/Public/<System>.h` and `Private/<System>.cpp`, following `GameClock.h`/`.cpp` for style (no Unreal dependency, `PERFECTWORLDCORE_API` export guard). Add the `.cpp` to `add_library(PerfectWorldCore STATIC ...)` in `CMakeLists.txt`. Write `Tests/<System>Tests.cpp`, then register it exactly like the existing two:
```
add_executable(PerfectWorld<System>Tests Tests/<System>Tests.cpp)
target_link_libraries(PerfectWorld<System>Tests PRIVATE PerfectWorldCore)
add_test(NAME PerfectWorld<System>Tests COMMAND PerfectWorld<System>Tests)
```
Run `python Scripts/build_core.py --build-dir C:/Users/DEBDUTTA/AppData/Local/Temp/pwcore` — CTest must now report one more passing executable than before, by name.

**Expose a portable system to Unreal through the subsystem.** Follow `Source/PerfectWorld/Public/PerfectWorldCharacterCreatorSubsystem.h`'s pattern: a `UGameInstanceSubsystem` (new or extended) that holds/owns the portable type. Mirror any `enum class` used across the boundary as a `UENUM(BlueprintType)` with an explicit comment that its entries must stay in the same order as the core enum (see `EPWCreationError`, `EPWResumePoint`). Mirror any plain-data struct as a `USTRUCT(BlueprintType)` with `BlueprintReadOnly` properties (see `FPWOpeningHomePlan`), converting `std::string`/`string_view` to `FName`/`FText` at the boundary. Expose operations as `UFUNCTION(BlueprintCallable)`. Verify at Rung 2 (editor build — confirms UHT accepts the reflection macros and the module links), then Rung 3 (PIE — confirms Blueprint can actually call it and gets the expected value back).

---

## 9. Definition of done

| Kind of change | Done when |
|---|---|
| **Portable-only** (Source/PerfectWorldCore, Tests/) | Rung 1 passes for both test executables; `STATUS.md` entry recorded; committed. |
| **Unreal-interface** (Source/PerfectWorld, subsystem/Blueprint exposure, no new visible behaviour) | Rung 1 (if portable code changed) and Rung 2 both pass; `STATUS.md` entry recorded; committed. |
| **Visible/interactive behaviour** (anything a player would notice in PIE) | Rungs 1–2 as applicable, **plus** Rung 3 actually run and its specific checklist recorded (not "looked fine" — the falsifiable steps, per `BUILD_GUIDE.md`'s checklist style); committed. |
| **Content-only** (a new problem family, event, nation, asset ID — data, no engine code) | Content validator passes; any new/extended CTest for portable rules it touches passes; `STATUS.md` entry recorded; committed. Does not require Rung 2/3 unless it also changes Unreal-facing code. |
| **Milestone** (`GAME_PLAN.md` §8) | Every step under it individually done per the rows above, **plus** the milestone's own stated gate condition met and recorded — a gate is not implied by its steps passing; check it explicitly (e.g., M4's blind-test gate is a separate owner-run session, not a rung). |
| **Save-schema bump** | All of "portable-only" above, plus migration tests from every still-supported prior version and rejection tests at both boundaries (§8 recipe). |

A step recorded with any field in §2's format missing, or with a rung claimed but not actually run, is not done — fix the record, don't just note it and move on.

---

## 10. Session start and handoff

### Starting cold

1. Read `Docs/GAME_PLAN.md` in full (it is short enough to reread each session and is the authority on everything else).
2. Read `Docs/ROADMAP.md` for the current step queue and `Docs/STATUS.md` for the last recorded state.
3. `git -C C:/Users/DEBDUTTA/Desktop/PerfectWorld status` — confirm the working tree matches what `STATUS.md` claims. Uncommitted changes from a prior session that were never recorded are a red flag; reconcile before starting new work.
4. Confirm green: run Rung 1 (`python Scripts/build_core.py --build-dir C:/Users/DEBDUTTA/AppData/Local/Temp/pwcore`) before touching anything, so a pre-existing break is not mistaken for something the new session caused.
5. Confirm editor state: is `UnrealEditor.exe` running against this project? If a build is planned this session, it must be closed first (§4); if PIE testing is planned, note whether the owner needs to be the one driving it.
6. Check `Docs/GAME_PLAN.md` §12 for open questions that might block the planned step, and this document's escalation rule (§1) if the step turns out to touch one.

### Ending a session

1. Every step attempted this session has a complete `STATUS.md` entry per §2's six fields — including any step left half-done, recorded honestly as such with its actual remaining limitation, not silently dropped.
2. Every commit made is real and matches what `STATUS.md` claims — no "will commit later."
3. `git status` is clean or every remaining change is either intentionally uncommitted generated churn (§4) or explicitly called out in the last `STATUS.md` entry as unfinished work-in-progress with a stated reason.
4. If a build or the editor was left running, say so, so the next session (or the owner) does not hit the "two Unreal builds at once" or "editor locks the DLL" traps.
5. If anything was escalated to the owner per §1, it is visible in `STATUS.md` or `GAME_PLAN.md` §12, not only in a chat transcript that will not persist.

---

## 11. Documentation rules

**Ownership**, per `GAME_PLAN.md`'s own table (repeated here for convenience — that table is the authority if this one ever drifts):

| Document | Owns |
|---|---|
| `VISION.md` | Why the game exists; what must never be compromised |
| `GAME_PLAN.md` | What the game is, its full flow, milestones, the decision log |
| `SYSTEMS.md` | Every system: data it owns, commands, events, dependencies, save data |
| `ASSET_PIPELINE.md` | Voxel placeholder style, asset IDs, the swap contract, both import routes |
| `DEVELOPMENT_WORKFLOW.md` (this document) | How the owner and AI agents build, verify and record work |
| `ROADMAP.md` | Every milestone broken into bounded, verifiable steps |
| `ALTERNATE_WORLD.md`, `GLOBAL_WORLD.md`, `PATHS_AND_PROGRESSION.md`, `AWAKENING_AND_EXAM.md`, `SCHOOL_AND_PROGRESSION.md`, `CHARACTER_CREATION.md`, `INDIA_WORLD_BIBLE.md` | Their named subject, per `GAME_PLAN.md` §0 table |
| `STATUS.md` | The running, dated record of what was actually done and verified this session — not a design document |

If a fact seems to belong in two places, it lives in the one that owns it and the other links to it — do not duplicate rules across documents, since duplicates drift.

**Keep the `GAME_PLAN.md` decision log (§10) current.** Any decision that changes scope, a core mechanic, or a previously-recorded rule gets a dated row there the same session it is made, in the same terse style as the existing entries. A decision made in chat and not logged did not happen, for the next session's purposes.

**Mark superseded text; do not silently delete it.** When a document's content is overtaken by a later decision, follow `MASTER_PLAN.md`'s own pattern at its top (the "Scope superseded, 2026-09-20" block): a dated note stating what changed, what specifically is superseded, and what — if anything — still holds and why. Delete only text that was simply wrong, never text that was once-true design history.

**Keep every document under 500 lines.** If a document is approaching the limit, split it along an existing seam (a named subsystem, a milestone) rather than compressing prose to fit — compressed prose loses the precision these documents exist for.

**On finding contradictions between documents:** do not silently pick one. Note the conflict where you find it (a one-line comment or a `STATUS.md` note), and if it is a live blocker for the step at hand, resolve it toward whichever document's table above owns that fact — `GAME_PLAN.md` wins if the conflict is with `GAME_PLAN.md` itself, per this document's own header.
