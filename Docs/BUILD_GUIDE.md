# Build guide

Use the installed Unreal Engine5.7 and Visual Studio2022 C++ Build Tools. The standalone core uses CMake3.20+ and C++20. Python3 is used only to invoke repeatable build commands with a normalized Windows environment; it is not a runtime game dependency.

## Portable rules

Open a terminal in the project folder:

```powershell
python Scripts/build_core.py --build-dir build/core
```

This configures CMake, builds Release and runs CTest. The included runner removes duplicate case variants of environment keys in child processes because this host supplied both Path and PATH, which MSBuild rejected. No system environment settings are changed.

The standalone suite checks age16, budget totals, default preset, invalid IDs/ranks, exact refunds, over-budget atomic rejection, four-premium cap, immutable started origin, reset and versioned in-memory record validation. It also exercises10,000 deterministic edit sequences. It does not test the Unreal UI, disk persistence or school gameplay.

## Unreal Editor module

```powershell
python Scripts/build_unreal.py
```

For a different engine location:

```powershell
python Scripts/build_unreal.py --engine-root 'D:/Epic/UE_5.7'
```

The target uses BuildSettingsVersion.V6 for the installed5.7 engine. Engine header tools generate reflection code. The portable module exports its public API when linked as an Unreal DLL; CMake builds use the same source with no engine dependency.

After a successful build, open PerfectWorld.uproject with Unreal5.7. The engine Entry map is the default. Play In Editor should instantiate PerfectWorldMenuGameMode and show the creator. This is a development menu, not a finished title screen. Test in a viewport with input focus.

Manual smoke checklist, to be completed in the next small step unless STATUS explicitly records otherwise:

1. The heading says Kolkata, Nabapara Vidyalaya, Class XI, Age16.
2. Balanced Scholar fills exactly1,000 points.
3. A higher rank that overspends is rejected without changing the draft.
4. Lowering a rank restores its cost difference; it can be raised again.
5. A fifth rank4 is rejected even if total spend is under1,000.
6. Confirm Origin is enabled only for an exact valid allocation.
7. Confirm locks trait changes and displays that school/world gameplay is future work.
8. New Draft resets the creator. No file is saved and relaunch starts fresh.

Full controller/touch navigation and the complete appearance editor are planned. Slate labels are currently prototype English strings, not production localized text.

## Files and snapshots

Source/PerfectWorldCore owns the portable rules. Source/PerfectWorld adapts them to Blueprint and displays the prototype menu. Tests contains the standalone executable. Config selects the menu game mode and a conservative rendering baseline. Docs describes the intended game. Art is concept reference only.

CaptureSave/RestoreSave operate on C++ values in memory. The current schema stores a fixed-order20-rank array. Before changing trait order or shipping persistent files, replace this with stable-ID serialization plus migration; current version1 records must not be silently reinterpreted. Reset creates a new draft and intentionally discards its prior origin. Neither helper is a gameplay progression save service.

Do not copy Intermediate, Saved, DerivedDataCache or generated build folders into source control. Binary art/Unreal assets can use Git LFS once the repository is initialized. No remote repository has been created by this starter task.

The build script disables the optional Unreal Build Accelerator (-NoUBA) so local compilation does not depend on its global cache permissions. This uses the standard parallel compiler executor.

