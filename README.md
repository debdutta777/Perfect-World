# Perfect World: Awakening

**Starter project — Kolkata, West Bengal • Age16 • Class XI • 1,000 creation points**

*Perfect World: Awakening* is a first-person life simulation and role-playing game, switchable to third person, set in a fully alternate near-future Earth where Gates are a normalised hazard fifty real nations have organised their societies around. The player begins at sixteen in Kolkata and may live an entire life, in any direction — the Gates are the setting, not the job; a player may never fight anything and still have a complete game. The whole game is developed with voxel block visuals — cuboids with pixel textures, a look rather than a voxel engine — until the game is complete; real stylised 3D art is integrated afterward. The working title comes from the supplied concept. The opening uses real **Kolkata** with fictional **Nabapara** and **Nabapara Vidyalaya**. The first playable scope is one neighborhood; West Bengal, India and then the wider world form the expansion plan.

## What is included

- Unreal Engine5.7 C++ project and a small native Slate character-creation menu.
- Portable C++20 rules:20 traits, ranks0–4, cumulative costs0/25/60/110/180,1,000-point budget, maximum four rank-4 traits, exact age16, free identity/cosmetics, reversible draft edits and validation.
- One implemented1,000-point Balanced Scholar preset. Other documented presets and all trait gameplay effects are design specifications for future work.
- Five free, point-cost-free household templates (parent, extended family, grandparent, adult sibling, appointed guardian), selected at Household Resources Ranks 1–4 and locked once the prologue starts. As of a 2026-09-20 design decision, Household Resources Rank 0 now means institutional care (a children's home) instead of an ordinary baseline — see Docs/CHARACTER_CREATION.md and Docs/STATUS.md. The home scenes and art for every one of these six outcomes do not exist yet.
- Versioned creation records, serialized as text by stable trait ID, saved to three local JSON slots under `Saved/SaveGames/`. This is disk save/load for the creation record only, not a complete long-term life save system, and it has not yet been checked through a Play-In-Editor smoke test.
- Detailed game design, creator specification, Bengal world bible, names, Unreal architecture, art direction, production plan and small-step queue.
- Two generated concept sheets: Kolkata neighborhood and student costumes. They are reference art, not playable3D assets.

Private GitHub repository: [debdutta777/Perfect-World](https://github.com/debdutta777/Perfect-World).

## Start here

**Read [GAME_PLAN](Docs/GAME_PLAN.md) first.** It is the master plan and the single entry point: what the game is, its full flow from boot to the end of a life, its presentation (voxel block visuals during development, first-person camera by default), its systems, its milestones and its decision log. Where any older document conflicts with it, `GAME_PLAN.md` wins.

**First time in Unreal? Follow [UNREAL_QUICKSTART](Docs/UNREAL_QUICKSTART.md).** It walks through building, opening the project and checking the creator controls.

1. Read [GAME_PLAN](Docs/GAME_PLAN.md), then [ROADMAP](Docs/ROADMAP.md) for the current bounded steps.
2. Follow [BUILD_GUIDE](Docs/BUILD_GUIDE.md) for standalone tests and Unreal compilation.
3. Read [CHARACTER_CREATION](Docs/CHARACTER_CREATION.md), [AWAKENING_AND_EXAM](Docs/AWAKENING_AND_EXAM.md) and [INDIA_WORLD_BIBLE](Docs/INDIA_WORLD_BIBLE.md).
4. See [SYSTEMS](Docs/SYSTEMS.md), [ASSET_PIPELINE](Docs/ASSET_PIPELINE.md), [TECHNICAL_ARCHITECTURE](Docs/TECHNICAL_ARCHITECTURE.md) and [ART_DIRECTION](Docs/ART_DIRECTION.md).
5. Check [STATUS](Docs/STATUS.md) for exactly what has been verified. `GAME_DESIGN.md`, `PRODUCTION_PLAN.md` and `NEXT_SMALL_STEPS.md` are superseded records, kept for history — see the notice at the top of each.

## Current boundary

**Intended flow (design, not yet fully built or verified):** a title screen (New Game / Load Game / Settings / Exit) leads New Game into an appearance editor showing a block character in an orbit camera, then point allocation including household (with Household Resources Rank 0 meaning institutional care rather than an ordinary baseline), then into a starting room chosen by the confirmed household template and rank, playable in first person by default and switchable to third person. See `GAME_PLAN.md` §4 for the full flow and §11 for what is built today, and `Docs/STATUS.md` for the decision record.

**What actually exists today** (restated honestly from `GAME_PLAN.md` §11): character creation rules (the 1,000-point ledger, five households, rank-0 care, the opening-home plan), an engine-neutral appearance record, three JSON save slots with schema migration and resume points, the game clock, and a front end (title, load, settings, appearance editor, household and points panel) — covered by 225,374 creation assertions and 16,857 clock assertions. **Not one of those screens has been exercised in a launched window.** That verification gap is the project's largest current risk and the first step of any work after the plan. No world, room, character model, NPC, activity or gameplay exists yet. There is no packaged or cooked build.

Windows is the development host, per the owner's decision log; Android is a later target, not the primary design driver. Other platforms need their own tooling and tests. See the architecture document for Epic documentation and limits.

## Model workflow

Per `GAME_PLAN.md` §9: Claude integrates and implements in the repository; GPT-6 (`gpt-6-astra` via Codex) critiques design; GPT-5.6 Sol handles routine implementation when used. `Docs/DEVELOPMENT_WORKFLOW.md` is authoritative on how work gets done. Work one small change at a time with an explicit check. Model execution notes are in STATUS.md.

## Art

![Kolkata neighborhood concept](Art/kolkata-nabapara.png)

![Student costume concepts](Art/student-lineup.png)

See [ART_PROVENANCE](Docs/ART_PROVENANCE.md) for generation notes and the student sheet's noncanonical background.
