# Lane acceptance runner (AW-02)

Recovery notes, 2026-10-18, before implementation: read AGENTS.md, AW-02 order,
AW-01 driver and package_windows.py. Only this document and the new runner are
owned. No commit, Unreal build or game launch is authorised for this worker.
The requested D:/PWWork/logs/AW-02_notes.md is outside the allowed worktree;
these recovery notes substitute for it. Completion: standard-library runner,
save protection, evidence checks, no-game self-test, integrator command.

Engine source inspection: UE 5.8 Core/Private/Misc/Paths.cpp uses UserDir in
ProjectUserDir and appends Saved[_suffix] in ProjectSavedDir. Production saves
use ProjectSavedDir/SaveGames. Packaged runtime behavior remains unverified.

Implementation checkpoint: runner written with PNG filter decoding, strict result
validation, fresh evidence folders, package hashing, serial execution, persistent
save backups and recovery journal. No game launched. Next: execute self-test and
review interruption handling before handoff.

## First integrator run

Use Python 3.13 in a normal terminal, from this worktree. First run:

```powershell
python Scripts/run_lane_acceptance.py --self-test
```

Then, after packaging the integrated Development candidate (the runner does not
build it), run the bounded offscreen spike once:

```powershell
python Scripts/run_lane_acceptance.py --package D:\PWBuilds\Windows --scenario boot_walk_interact --out D:\PWWork\worktrees\w-aw02\Saved\LaneAcceptance\boot-first --commit 337593c --timeout-s 240 --start new
```

Replace the package path if the integrator archives elsewhere. Replace
`337593c` with the actual packaged candidate's full SHA; this example names the
assigned integration baseline, not a claim about the existing package. Omit
`--commit` to use `git rev-parse HEAD` from the runner's repository. Git must be
available in that terminal. Always use a fresh, empty output directory.
`--scenario` is repeatable; only one game process runs at a time. Default
`--timeout-s` is 240 per launch. `--start` defaults to `auto` and accepts `new`
and `continue`. Each ordinary scenario gets separate private saves; `continue`
on that initially empty directory will honestly fail. Seeding private saves is
not implemented; restart-pair is the supported new-to-continue sequence.

The runner discovers the single inner
`PerfectWorld/Binaries/Win64/PerfectWorld.exe` and launches it directly. The
root bootstrap executable is not used: waiting for a launcher alone would not
prove the engine exited. All launches include:

```text
-RenderOffScreen -unattended -nosplash -nosound -windowed -ResX=1280 -ResY=720 -log
-UserDir=<private scenario user directory>/
-PWAcceptance=<scenario> -PWAcceptanceOut=<phase evidence directory>
-PWAcceptanceCommit=<sha> -PWAcceptanceStart=<mode> -PWAcceptanceSlot=0
```

No NullRHI. Review images from this first spike for the expected production
scene and check that desktop focus/input were unaffected. Pixel variance alone
does not prove usable rendering or focus isolation on this VM. If the spike
fails, retain evidence and use the owner-approved window described in AW-01's
plan; this runner does not schedule or approve such a session.

## Save protection and recovery

The production subsystem writes `ProjectSavedDir/SaveGames`. Installed UE 5.8
`Engine/Source/Runtime/Core/Private/Misc/Paths.cpp`, `ProjectUserDir`,
`GameSavedDir` and `ProjectSavedDir` show that `-UserDir=` redirects its base.
`-saveddirsuffix=` is also supported in that source, but is not used here.
Source inspection is not packaged runtime proof, so backup protection is always
enabled until the integrator has that proof, and remains enabled afterwards.

Before any game launch the runner copies and SHA-256 verifies both normal
locations: `<package>/PerfectWorld/Saved/SaveGames` and
`%LOCALAPPDATA%/PerfectWorld/Saved/SaveGames`. Supply repeatable
`--saved-dir <absolute SaveGames folder>` for any custom build's other known
save location; a renamed project or custom engine path override must be audited
by the integrator before use. These arguments protect additional locations;
they do not redirect the game. Linked save paths and overlapping protected
directories are refused. Close any PerfectWorld game first. A persistent lock
under `%LOCALAPPDATA%/PerfectWorld` prevents concurrent runners for this user.

Backups live in `<out>/save-backups/<UTC timestamp>/<index>` and are retained.
`save-protection.json` records original absence, relative entries, file hashes,
backup paths and restoration state. A backup must match both the original
manifest and a second read of the source before the runner may launch the game.
Owner saves stay in place. Acceptance uses a fresh private user directory at
`<out>/<scenario>/user/Saved/SaveGames`. This also prevents ordinary scenarios
from sharing test saves accidentally.

After each launch, the runner requires a save file in that private directory
and unchanged normal save manifests. This is observed isolation evidence; the
integrator should additionally inspect logs (or a file-access trace) to confirm
the effective ProjectSavedDir, and compare the before/after owner manifests.
An ignored redirection flag cannot silently pass: missing private saves or
changed owner saves prevent PASS. To explicitly test fallback or an ignored
flag, use a disposable Windows profile/package with fixture slots, never the
owner's only copies. No packaged isolation experiment was run by this worker.

The `finally` path restores changed owner folders even on timeout, crash or
Ctrl+C. Restoration checks the backup again, prepares a verified replacement,
renames the current folder to a timestamped `.aw02-retained-*` sibling, and
installs the replacement. Originally absent folders return to absence. No save
folder or backup is deleted. Restoration failures retain the lock, journal,
backups and test artifacts for recovery. The runner refuses restoration while
PerfectWorld.exe is still running.

Host power-off or forced termination of Python cannot execute `finally`. After
reboot, ensure the old runner and game are stopped, then restore before any new
run (replace the output with the interrupted run's actual directory):

```powershell
python Scripts/run_lane_acceptance.py --recover D:\PWWork\worktrees\w-aw02\Saved\LaneAcceptance\boot-first
```

Recovery verifies the retained backup and is repeatable if interrupted itself.
It never resumes acceptance or turns interrupted evidence into PASS. If backup
verification fails, stop and inspect retained copies; the runner refuses to
overwrite owner saves. Do not manually remove the lock to bypass recovery.
This protects against interrupted software operations, not physical disk loss.

## Evidence and meaning of PASS

Each launch has `<out>/<scenario>/<single|part1|part2>/` containing AW-01's
`<scenario>.json`, `<scenario>__<step>.png`, captured `console.log`, and copies
of available `PerfectWorld.log` files. Numbered logs identify the private user
location (0), package Saved (1), and normal user Saved (2); fallback logs may
predate this launch, so use console/private logs for freshness. Partial logs
are copied after failed and timed-out launches too. Never discard the partial
driver JSON. `summary.json` and `summary.md` are updated between launches and
before launch, so a power-off leaves an INCOMPLETE record. Summary and journal
writes use same-directory replacement with a flushed temporary file.

PASS requires all requested launches to exit with code zero within the timeout,
schema 1 with matching scenario/commit, `completed` and `passed` true, every
planned step exactly once in order, and all step assertions true. At least one
rendered screenshot is required. Every referenced screenshot must use the
contracted filename, be larger than 1 KB and decode as a non-interlaced 8-bit
RGB/RGBA PNG with nonzero RGB pixel variance. CRCs, dimensions, decompressed
lengths and all five PNG filters are checked with standard-library zlib.
Unsupported PNG types fail closed. Solid colours, including solid red, fail.

The runner records the launched executable's SHA-256 and a package manifest of
executables, DLLs and cooked pak/utoc/ucas files. It compares identity before and
after each launch, including between restart parts. The echoed commit is
checked, but does not prove what source produced the package; the integrator
must retain the packaging provenance. The driver supplies `planned_steps`;
this runner validates that plan's execution, not the adequacy of its coverage.

Timeouts terminate the process tree and wait for the direct executable to stop;
timeouts/nonzero exits, missing/unparseable results and interrupted runs are
INCOMPLETE even if a partial driver file says passed. Failed assertions or
changed package/save artifacts cannot PASS. Return codes: 0 PASS (or successful
self-test/recovery), 1 attempted acceptance did not pass, 2 setup/CLI error.
Read summary errors and the restoration journal for the precise reason.

AW-01 currently implements only `boot_walk_interact` and `stationery_shift`.
Its stationery script is a skeleton that walks, opens the door, captures the
interior and observes NPC positions. It does not yet prove Mira's full behavior
or the other reserved scenarios. Automated PASS is not a human test of feel,
controller use, readability, focus safety or complete lane acceptance. The
integrator reviews the images before recording automated packaged acceptance.

## Future restart pair

```powershell
python Scripts/run_lane_acceptance.py --package D:\PWBuilds\Windows --scenario save_restart --restart-pair --out D:\PWWork\worktrees\w-aw02\Saved\LaneAcceptance\restart-first --timeout-s 240
```

The pair launches the identical inner executable twice with the same private
user directory and slot 0: part 1 passes `Start=new`, part 2 `Start=continue`.
Each has separate evidence so part 2 cannot overwrite part 1. Part 2 starts
only after part 1's successful evidence validation and clean process exit.
`--restart-pair` accepts only `save_restart` and the default `--start auto`;
the runner selects new/continue itself. No in-process world restart substitutes
for the second launch. AW-01 must implement that scenario and use Start to
select its save/verify phases; no new phase flag is invented here. Currently
the driver reports this reserved scenario as unknown, so this command cannot
PASS yet.

## Worker verification and recovery notes

Only the two AW-02 owned files were edited. No Docs/STATUS.md edit was made
because it is outside this order's ownership. No commit, Unreal build or game
launch was performed. The requested external notes path remained untouched.
The installed Python 3.13 Store executable is access-denied in this sandbox;
the accessible bundled Python is 3.11.8. Final self-test results follow below.

Final checkpoint, 2026-10-18: ran the script with that bundled Python 3.11.8:

```text
SELF-TEST PASS: 41 checks (PNG filters, evidence parser, save backup/restore).
No game launched; helper exit/timeout checks passed. Packaged offscreen/restart remain unverified.
```

The self-test creates all fixtures under ignored `Saved/AW02SelfTest`, including
fake owner slots, and cleans its temporary fixture directory. Checks include
clean/nonzero helper exits, killing a stalled Python helper, a missing contracted
screenshot, corrupt PNG data, a damaged backup refusal, restoring existing slots
and restoring original absence. `--help` also ran successfully. Python 3.13
execution, actual packaged rendering/isolation, and the future restart scenario
remain integrator verification. No Unreal checks were attempted.
