# Build guide

Use the installed Unreal Engine 5.8 and the Visual Studio 2026 C++ Build Tools (MSVC 14.50, Epic's preferred toolset for 5.8; the build tool finds it on its own). The project moved from 5.7 to 5.8 on 2026-09-25. The portable suite configures a fresh build folder with the newest Visual Studio installed, and an existing folder keeps its original generator. The standalone core uses CMake 3.20+ (4.2+ to generate for VS 2026) and C++20. Python 3 is used only to invoke repeatable build commands with a normalized Windows environment; it is not a runtime game dependency.

## Setting up a new machine, such as a cloud PC

**The machine needs:** 64-bit Windows 10 22H2 or Windows 11; **a GPU with DirectX 12**, because the editor is unusable without one and basic cloud PCs have none, so choose a GPU instance; 32 GB of RAM recommended (16 GB works, but builds then compile one file at a time); and about 150 GB free for the engine, the build tools and the shader cache. If the system drive is small, put everything on a second drive: on the cloud PC the repository is `D:\Dev\PerfectWorld`, the engine `D:\Epic Games\UE_5.8` and the Build Tools `D:\VS\BuildTools`. The Epic Launcher MSI ignores winget `--location`; install it with `msiexec /i <msi> INSTALLDIR="D:\Epic Games\"`, and choose the engine folder in the launcher's install dialog.

1. Git, Git LFS, Python and CMake: `winget install Git.Git GitHub.GitLFS Python.Python.3.13 Kitware.CMake`, then `git lfs install`.
2. Clone the private repository after signing in to GitHub: `git clone https://github.com/debdutta777/Perfect-World.git`.
3. The C++ toolchain, approving the administrator prompt yourself:
   `winget install --id Microsoft.VisualStudio.BuildTools --exact --override "--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --add Microsoft.VisualStudio.Component.VC.14.50.18.0.x86.x64 --add Microsoft.Net.Component.4.8.1.SDK --add Microsoft.Net.Component.4.8.1.TargetingPack"`
   The .NET Framework SDK is required: without it the Unreal build stops with `Result: Failed (RulesError)` and "Could not find NetFxSDK install dir" (found on the cloud PC, 2026-09-28). To add it to an existing install: `"C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe" modify --installPath <Build Tools folder> --add Microsoft.Net.Component.4.8.1.SDK --add Microsoft.Net.Component.4.8.1.TargetingPack --passive --norestart` (no `--wait`; `setup.exe` rejects it with exit code 87).
4. The Epic Games Launcher (`winget install EpicGames.EpicGamesLauncher`), then **Unreal Engine 5.8** from it with an Epic login. For an install outside `C:/Program Files/Epic Games/UE_5.8`, pass `--engine-root` to the build script.
5. Rung 1: `python Scripts/build_core.py --build-dir <a short path>`. Both test executables must pass.
6. Rung 2: `python Scripts/build_unreal.py`. The result must be Succeeded.
7. Open `PerfectWorld.uproject` with 5.8. About 35 seconds after launch, the editor's MCP server listens on `127.0.0.1:8000`. The first launch also compiles shaders, which takes a while.
8. Install Claude Code (`irm https://claude.ai/install.ps1 | iex` in PowerShell) and start it in the project folder. It reads `.mcp.json` and asks once to approve the `unreal` server; after that, `claude mcp list` shows it connected. Start Claude Code after the editor, or reconnect with `/mcp`.

Nothing under `Binaries/`, `Intermediate/`, `Saved/` or `DerivedDataCache/` is in the repository, so a new machine always builds from source. Saved games stay on the machine that wrote them.

## Portable rules

Open a terminal in the project folder:

```powershell
python Scripts/build_core.py --build-dir build/core
```

This configures CMake, builds Release and runs CTest. The included runner removes duplicate case variants of environment keys in child processes because this host supplied both Path and PATH, which MSBuild rejected. No system environment settings are changed.

The standalone suite checks age16, budget totals, default preset, invalid IDs/ranks, exact refunds, over-budget atomic rejection, four-premium cap, immutable started origin, reset and versioned in-memory record validation. It also exercises10,000 deterministic edit sequences, the household rules, and the save text format: an escaped round trip, every truncation of a record, a renamed trait, corrupt text, an unknown household, version-1 migration and out-of-range slots. It does not test the Unreal UI or school gameplay, and it does not touch the file system. Reading and writing slot files lives in the Unreal subsystem and has no automated coverage yet.

The suite also runs the content validator: `pwcontent validate Data/Content` must pass, and each broken fixture under `Tests/Fixtures/Content/` must fail for the reason it was written to show (ROADMAP M1-06). Run it directly with `D:/pwcore/Release/pwcontent.exe validate Data/Content` after a build.

## Unreal Editor module

```powershell
python Scripts/build_unreal.py
```

For a different engine location:

```powershell
python Scripts/build_unreal.py --engine-root 'D:/Epic/UE_5.8'
```

Both targets use `BuildSettingsVersion.V7`, which 5.8 requires: with V6 the editor target changes warning levels shared with the installed `UnrealEditor` and the build tool refuses it. V7 also makes missing return values, dangling references and unreachable code errors, and uses precise floating point for the editor. Engine header tools generate reflection code. The portable module exports its public API when linked as an Unreal DLL; CMake builds use the same source with no engine dependency.

After a successful build, open PerfectWorld.uproject with Unreal 5.8. The engine Entry map is the default and there is no game mode of our own yet: the flat Slate menu was deleted on 2026-09-28 (`ROADMAP.md` M0-13a), and the title and creation screens return in 3D with M1-23. Until then Play In Editor shows an empty Entry map, which is expected.

## Files and snapshots

Source/PerfectWorldCore owns the portable rules. Source/PerfectWorld adapts them to Blueprint through `UPWCharacterCreatorSubsystem`. Tests contains the standalone executables. Config holds a conservative rendering baseline and the Unreal MCP settings. Docs describes the intended game.

CaptureSave/RestoreSave operate on C++ values in memory. Persistent creation records are written by `Source/PerfectWorldCore/Private/CreationSaveSerialization.cpp` as text, a JSON subset, with trait ranks keyed by stable trait ID rather than array position — a renamed or reordered trait fails loudly instead of silently shifting ranks. The schema is version 4 (`SaveSchemaVersion`), and every version from 1 upward still loads (`MinSupportedSaveVersion`). Each step migrates explicitly and never reinterprets old data in place. Version 1 records gain the default household (`parent_household`). Records before version 3 carry free-form cosmetic strings that have no defined mapping to appearance option IDs, so they load with the default appearance. Records before version 4 carry no resume point, so a confirmed origin resumes in world and an unconfirmed draft resumes at allocation. A record naming an unknown household, an unknown appearance option or an unknown resume point is refused outright, and any version above 4 is refused as unsupported. The CTest suite covers every migration step. There are three save slots, `PerfectWorld::SaveSlotCount ==3`, written as `creation_slot_1.json` through `creation_slot_3.json` under the project's `Saved/SaveGames/` directory. A save writes to a `.tmp` file first and moves it into place, so an interrupted save cannot destroy the previous record; a corrupt or missing file leaves the in-progress creation untouched and reports a player-readable message. Reset creates a new draft and intentionally discards its prior origin. None of this is a gameplay progression save service — it persists only the creation record.

Do not copy Intermediate, Saved, DerivedDataCache or generated build folders into source control. Binary art/Unreal assets can use Git LFS once the repository is initialized. The source repository is [debdutta777/Perfect-World](https://github.com/debdutta777/Perfect-World); build outputs remain local and ignored.

The build script disables the optional Unreal Build Accelerator (-NoUBA) so local compilation does not depend on its global cache permissions. This uses the standard parallel compiler executor.

