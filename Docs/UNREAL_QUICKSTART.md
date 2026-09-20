# Unreal Quick Start

This guide opens the Perfect World creator prototype in Unreal Engine 5.7. It does not create a playable school or city.

## Build and open the project

1. Use Unreal Engine **5.7** and Visual Studio 2022 with the C++ game development tools. Both were found on this PC, and the Desktop project compiled successfully on 19 September 2026.
2. Open PowerShell and go to the project folder:
   ```powershell
   Set-Location 'C:\Users\DEBDUTTA\Desktop\PerfectWorld'
   ```
3. Build the Unreal modules:
   ```powershell
   python Scripts/build_unreal.py
   ```
   Wait for the command to finish. If Python is not found, install Python 3 and reopen PowerShell.
4. In File Explorer, double-click `PerfectWorld.uproject`. If Windows asks which Unreal version to use, choose **Unreal Engine 5.7**. If asked to convert or upgrade the project, cancel and reopen with 5.7. First launch can take time while shaders are prepared.
5. Unreal opens the default **Entry** map, a blank engine map. In the top toolbar, click **Play** (or press **Alt+P**) to use **Play Selected Viewport**. The expected result is the character creator menu in the viewport.

## Try the creator

These main desktop mouse controls were checked on20 September2026. See [the smoke-test report and screenshot](MENU_SMOKE_TEST.md). The menu currently uses small prototype text; enlarging it is the next UI task.

- Click **Balanced Scholar preset**. It should set the traits to exactly **1,000 creation points**.
- Use each trait row's **+** and **−** buttons to raise or lower its rank. A change that exceeds the budget, or would give a fifth trait rank 4, should be rejected without changing the draft. Lowering a rank refunds its cost difference.
- Click **New Draft / Reset** to clear the draft and start over. This is an in-memory reset; it does not write a save file.
- **Confirm Origin** is enabled only when the allocation is valid. Click it to confirm and lock the origin record. The menu should explain that school/world gameplay is not included.
- Press **Shift+F1** to release the mouse from the game viewport. Press **Esc** to stop Play In Editor (PIE).

## Find and edit project files

- `Source/PerfectWorld` contains the Unreal menu and game-mode C++ code; `Source/PerfectWorldCore` contains portable character-creation rules.
- `Docs` contains the design and build notes. `Art` contains concept reference images; they are **not** imported into the map or playable 3D assets.
- To browse C++ classes in Unreal, open **Content Drawer** and enable **C++ Classes** in its settings/filter menu if that folder is hidden. Edit source files in an IDE such as Visual Studio or VS Code, then rebuild; the prototype is implemented in C++, not only in Blueprints.

For the portable rules and their standalone checks, run this from the project folder:

```powershell
python Scripts/build_core.py --build-dir build/core
```

This uses CMake to configure and build the C++20 core, then runs CTest. It does not build or test the Unreal UI.

## If something goes wrong

- **Missing modules** dialog: close Unreal, run `python Scripts/build_unreal.py` from the project folder, wait for a successful build, then reopen the `.uproject` with Unreal 5.7. If compilation fails, install/repair Visual Studio 2022 C++ Build Tools and the Windows SDK, then run the build again.
- **Wrong engine version**: close the project and reopen it with Unreal 5.7. Decline conversion/upgrade prompts; do not open and save it in another engine version.
- **Entry opens without the creator**: in **Edit → Project Settings → Maps & Modes**, set **Default GameMode** to `PerfectWorldMenuGameMode`, then restart Play In Editor.
- **Build failure**: use the full compiler output from PowerShell or the build log to diagnose it. Red messages in the Unreal editor alone may be stale or unrelated; check the full compiler errors before changing code.

## Current limits and GitHub

The project currently has no city or school world, school gameplay, or disk saves. The Entry map is blank, and confirming the origin only keeps a record in memory; closing the game loses it.

The first local commit is `f948f1f`. Check local history with `git log --oneline` from the project folder. The requested private GitHub repository/upload is pending explicit approval; no remote URL has been configured yet.
