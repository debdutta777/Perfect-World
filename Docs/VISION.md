# Vision

**Document status:** the North Star. Owner decision, 2026-09-20. Every other document answers to this one. Where anything conflicts with this, this wins.

## What the game is

**A fully alternate world, lived from the inside, in which the supernatural is optional.**

Near-future Earth. Gates are a normalised hazard that society has adapted around — with insurance, cordons, licensing, research, politics and rumour — the way it adapted to fire and flood. Thirty major nations matter. Nations are rated, ranked and compete. The world is genuinely global.

The player begins at sixteen in Kolkata and may live an entire life. **They may never fight anything.**

## The one rule that defines this game

**The Gates are the setting, not the job.**

In this world, most people are not hunters. They are teachers, clerks, nurses, shopkeepers, engineers, civil servants, farmers, journalists, parents. Their lives are shaped by the awakened world without ever entering it — through what it costs to insure a building, which neighbourhoods are cheap and why, which careers opened and which died, what the news is about, who did not come home.

A player who wants to be a corporate employee for forty years must be able to do that, and it must be a complete and satisfying game. Not a joke option. Not an inferior build. **A life.**

## Why this is worth building

Almost no game has done it. Power-fantasy settings put you at the centre by default; life sims rarely have a world with this much pressure in it. The combination — an ordinary life with real stakes, inside an extraordinary world that does not care whether you are special — is close to unoccupied ground.

It also makes the project's existing thesis land harder. *The state makes the ten days equal; the sixteen years before them were not.* That is a statement about class, access and opportunity. It means far more in a world where most people never become heroes, because that is the world most players actually live in.

## The planning premise

The owner's explicit position, recorded so it is understood as deliberate rather than naive:

> This scope is not achievable with today's tooling by a part-time solo developer. AI capability is advancing quickly and work of this kind is expected to become tractable. Plan the full vision now; build toward it in layers; let the tooling catch up.

The concern about solo scope was raised once and decided against. It is not restated. **These documents are written to make the vision work, not to argue with it.**

What follows from that premise, practically:

- **Plan at full scope, build in layers.** Every layer must be independently playable, so the project is never a half-finished monument.
- **Architecture matters more, not less.** A thirty-nation, multi-decade simulation is only tractable on clean foundations: one clock, one command reducer, stable-ID versioned saves, data-driven content, deterministic RNG. Those decisions are cheap now and impossible to retrofit. See `MASTER_PLAN.md` section 4, which survives unchanged.
- **Content must be data, not code.** Careers, competitions, events, nations and schedules are authored data validated by tools. This is what lets future tooling — or future collaborators — scale the content without touching the rules.
- **Write everything down.** A multi-year project with gaps between sessions runs on its documents.

## What must never be compromised

1. **You can opt out of the supernatural entirely and still have a full game.**
2. **No path is the lesser path.** A world-renowned researcher, a respected teacher, a shrewd shopkeeper and an S-rank hunter are all legitimate endgames with comparable weight.
3. **Ordinary life is played, not skipped.** The mundane is the subject, not the loading screen before the good part.
4. **The world runs without the player.** NPCs have careers, families, ambitions and deaths whether or not the player is watching.
5. **Dignity.** Poverty, institutional care, failure and ordinary work are depicted with respect, never as punishment or as shorthand for lesser.

## Reading order

**Updated 2026-09-23** to add `GAME_PLAN.md` as the entry point directly below this document, plus `SYSTEMS.md`, `ASSET_PIPELINE.md`, `DEVELOPMENT_WORKFLOW.md` and `ROADMAP.md`. This is a table update only; nothing above this section changed.

| Document | Authority over |
|---|---|
| `VISION.md` | This. The North Star |
| `GAME_PLAN.md` | The master plan: what the game is, its full flow, presentation, milestones, decisions |
| `SYSTEMS.md` | Every system: data it owns, commands, events, dependencies, save data |
| `GLOBAL_WORLD.md` | Nations, rankings, competitions, global tension |
| `PATHS_AND_PROGRESSION.md` | Life paths and their ladders |
| `ALTERNATE_WORLD.md` | Ordinary life simulation, the world living without the player |
| `AWAKENING_AND_EXAM.md` | Awakening, the tutorial dungeon, the tower examination |
| `SCHOOL_AND_PROGRESSION.md` | Classroom, assessment, latent stat growth |
| `ASSET_PIPELINE.md` | Voxel placeholder style, asset IDs, the swap contract, both import routes |
| `MASTER_PLAN.md` | Architecture, risks, AI capability split, build order |
| `DEVELOPMENT_WORKFLOW.md` | How the owner and AI agents build, verify and record work |
| `ROADMAP.md` | Every milestone broken into bounded, verifiable steps |
| `CHARACTER_CREATION.md` | The creation contract |
| `INDIA_WORLD_BIBLE.md` | Kolkata, West Bengal, cultural authority |
