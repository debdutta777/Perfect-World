# Build guide

Use the installed Unreal Engine5.7 and Visual Studio2022 C++ Build Tools. The standalone core uses CMake3.20+ and C++20. Python3 is used only to invoke repeatable build commands with a normalized Windows environment; it is not a runtime game dependency.

## Portable rules

Open a terminal in the project folder:

```powershell
python Scripts/build_core.py --build-dir build/core
```

This configures CMake, builds Release and runs CTest. The included runner removes duplicate case variants of environment keys in child processes because this host supplied both Path and PATH, which MSBuild rejected. No system environment settings are changed.

The standalone suite checks age16, budget totals, default preset, invalid IDs/ranks, exact refunds, over-budget atomic rejection, four-premium cap, immutable started origin, reset and versioned in-memory record validation. It also exercises10,000 deterministic edit sequences, the household rules, and the save text format: an escaped round trip, every truncation of a record, a renamed trait, corrupt text, an unknown household, version-1 migration and out-of-range slots. It does not test the Unreal UI or school gameplay, and it does not touch the file system. Reading and writing slot files lives in the Unreal subsystem and has no automated coverage yet.

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
8. New Draft resets the creator and re-enables the household rows.
9. The Household section lists five templates; clicking one moves the `[x]` marker and updates the opening room kit line. Remaining points do not change.
10. Confirm Origin disables the household rows along with the trait controls.
11. Each of the three save slots starts empty with Load and Delete disabled.
12. Save writes the slot; its description line updates and Load and Delete become available.
13. Load restores that record, including its household. Delete empties the slot again.
14. Relaunch and Load restores the saved origin, which the in-memory-only build could not do.
15. Corrupting a slot file by hand produces a readable message and leaves the creation in progress untouched.

Full controller/touch navigation and the complete appearance editor are planned. Slate labels are currently prototype English strings, not production localized text.

## Files and snapshots

Source/PerfectWorldCore owns the portable rules. Source/PerfectWorld adapts them to Blueprint and displays the prototype menu. Tests contains the standalone executable. Config selects the menu game mode and a conservative rendering baseline. Docs describes the intended game. Art is concept reference only.

CaptureSave/RestoreSave operate on C++ values in memory. Persistent creation records are written by `Source/PerfectWorldCore/Private/CreationSaveSerialization.cpp` as text, a JSON subset, with trait ranks keyed by stable trait ID rather than array position — a renamed or reordered trait fails loudly instead of silently shifting ranks. The schema is version2. A version1 record still loads and migrates to the default household (`parent_household`); a version2 record naming an unknown household is refused outright; version3+ is refused as an unsupported version. There are three save slots, `PerfectWorld::SaveSlotCount ==3`, written as `creation_slot_1.json` through `creation_slot_3.json` under the project's `Saved/SaveGames/` directory. A save writes to a `.tmp` file first and moves it into place, so an interrupted save cannot destroy the previous record; a corrupt or missing file leaves the in-progress creation untouched and reports a player-readable message. Reset creates a new draft and intentionally discards its prior origin. None of this is a gameplay progression save service — it persists only the creation record.

Do not copy Intermediate, Saved, DerivedDataCache or generated build folders into source control. Binary art/Unreal assets can use Git LFS once the repository is initialized. The source repository is [debdutta777/Perfect-World](https://github.com/debdutta777/Perfect-World); build outputs remain local and ignored.

The build script disables the optional Unreal Build Accelerator (-NoUBA) so local compilation does not depend on its global cache permissions. This uses the standard parallel compiler executor.

