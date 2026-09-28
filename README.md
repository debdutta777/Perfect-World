# Perfect World: Awakening

**A 3D block-world life simulation and role-playing game — Kolkata, West Bengal • Age 16 • Class XI • 1,000 creation points**

*Perfect World: Awakening* is a first-person life simulation and role-playing game, switchable to third person, set in a fully alternate near-future Earth where Gates are a normalised hazard fifty real nations have organised their societies around. The player begins at sixteen in Kolkata and may live an entire life, in any direction: the Gates are the setting, not the job, and a player may never fight anything and still have a complete game.

The world is built at world scale: many real cities as block cities, fictional places inside and between them, and a Deep Realm, the dungeon behind the Gates, ten times bigger than Earth's total surface (`Docs/WORLD_SCALE.md`). Everything is developed as 3D voxel-block gameplay (cuboids with pixel textures, a look rather than a voxel engine) until the game is complete. Real stylised art is added afterwards, behind stable asset IDs. The opening uses real **Kolkata** with fictional **Nabapara** and **Nabapara Vidyalaya**.

Private GitHub repository: [debdutta777/Perfect-World](https://github.com/debdutta777/Perfect-World).

## Start here

**Read [GAME_PLAN](Docs/GAME_PLAN.md) first.** It is the master plan and the single entry point: what the game is, its full flow from boot to the end of a life, its presentation, its systems, its milestones and its decision log. Where any other document conflicts with it, `GAME_PLAN.md` wins.

1. [ROADMAP](Docs/ROADMAP.md) for the current bounded steps, and [WORLD_SCALE](Docs/WORLD_SCALE.md) for cities and the Deep Realm.
2. [BUILD_GUIDE](Docs/BUILD_GUIDE.md) for setting up a machine, the portable tests and the Unreal build.
3. [DEVELOPMENT_WORKFLOW](Docs/DEVELOPMENT_WORKFLOW.md) for how work is done and verified.
4. [SYSTEMS](Docs/SYSTEMS.md) and [ASSET_PIPELINE](Docs/ASSET_PIPELINE.md) for architecture, the block style and asset IDs.
5. [CHARACTER_CREATION](Docs/CHARACTER_CREATION.md), [AWAKENING_AND_EXAM](Docs/AWAKENING_AND_EXAM.md), [GLOBAL_WORLD](Docs/GLOBAL_WORLD.md) and [INDIA_WORLD_BIBLE](Docs/INDIA_WORLD_BIBLE.md) for the design.
6. [STATUS](Docs/STATUS.md) for exactly what has been verified.

## What exists today

Portable C++20 rules, compiled into Unreal Engine 5.8 and covered by two standalone test executables (225,395 creation checks, 16,857 clock checks): the 1,000-point character-creation ledger with five household templates and institutional care at Household Resources rank 0, an engine-neutral appearance record, save rules for three JSON slots with schema migration and resume points, and the game clock. `UPWCharacterCreatorSubsystem` bridges those rules to Unreal.

**Nothing is on screen yet.** The earlier flat Slate menus and 2D concept art were deleted on 2026-09-28 when the project moved fully to 3D block gameplay. No world, character model, NPC or activity exists yet; the next steps are the walkable block world (M1) and the world-scale lane.

Windows is the development host. Android is a later target.

## Model workflow

Per `GAME_PLAN.md` §9: Claude integrates and implements in the repository; GPT-6 (`gpt-6-astra` via Codex) critiques design; GPT-5.6 Sol handles routine implementation when used. Work one small change at a time with an explicit check.
