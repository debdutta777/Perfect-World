# Master production plan

**Document status:** production authority, 2026-09-20. Synthesised from a GPT-6 (`gpt-6-astra`) design critique and a GPT-5.6 Sol production review, reconciled against the owner's stated goal. Supersedes `PRODUCTION_PLAN.md` where they conflict.

**Owner's stated goal:** a generational game at AAAA scale, 2–3 years acceptable, artists hired later, free or generated art until then. Working part-time, most evenings.

> **Scope superseded, 2026-09-20.** After this plan was written the owner decided the main story must be **world level, not one district**: thirty major nations relevant (fifty since 2026-09-25), yearly country competitions at several hunter levels, open and university-level tower competitions, worldwide individual, university and national rankings, and a national "country level" that scales rewards. See `GLOBAL_WORLD.md` and `PATHS_AND_PROGRESSION.md`, which are authoritative on scope.
>
> The concern that a solo part-time developer cannot build a thirty-nation open world was raised once and the owner decided against it. It is not restated further; the global documents are written to make that vision work rather than to argue with it.
>
> **What survives from this document unchanged and still binding:** every architecture decision in section 4, the AI capability split in section 5, the five risks in section 6, and the immediate build order in section 7. Those are independent of scope and become *more* important as scope grows, not less. Sections 2 and 3 — the district caps, the 6–8 hour target, the "end at the examination" cut list — are superseded and are kept only as the record of a rejected alternative.
>
> **Addendum, 2026-09-23.** `GAME_PLAN.md` is now the master plan and the single entry point; where anything here conflicts with it, `GAME_PLAN.md` wins. Section 4's architecture decisions are restated as binding on everything in `GAME_PLAN.md` §6, and their full detail is migrating to `Docs/SYSTEMS.md` — this document is no longer their sole home, only their original record. The "Characters" row of section 4 anticipated "low-poly placeholders until the rig exists"; the owner has since made that concrete as voxel block visuals for the whole of development (`GAME_PLAN.md` §3, §8 M9), specified in `Docs/ASSET_PIPELINE.md`. Section 7's immediate build order is superseded by `Docs/ROADMAP.md`, which breaks the current milestones (`GAME_PLAN.md` §8) into bounded steps.

## 1. The honest position

One person plus AI agents cannot produce a AAA-scale open-world action RPG. AI writes code and tests quickly; it does not supply the hundreds of person-years of art, animation, level design, writing, audio, tuning, QA and platform support that scale requires. Pursuing that scope directly produces a permanently unfinished framework, which is the most common way ambitious solo projects die.

**But scale is not what makes a game generational.** *Disco Elysium* is one city block. *Papers, Please* is one desk. *Undertale* was substantially one person. *Hades* came from a small team. Each is remembered because one idea was executed without compromise.

This project already has that idea: **the state makes the ten days equal; the sixteen years before them were not.** That thesis is generational-grade. It does not need a nation to express it — it needs one neighbourhood, one school, one examination, executed perfectly.

So the plan below is not a reduction of the ambition. It is the only route that reaches it.

### Realistic schedule

| Milestone | Full-time | Part-time (evenings, ~15 h/wk) |
|---|---|---|
| Vertical slice (75–100 min, cooked) | 12–18 months | 2.5–3.5 years |
| The capped game | 3–5 years | Needs a team and budget |

The vertical slice is the decision point. If it proves the loop, it is also the thing that attracts collaborators, funding or a publisher — which is how the larger ambition becomes reachable at all.

## 2. Scope verdict

A premium, fully 3D, stylized narrative schedule RPG about one student's final weeks before the Unified Examination, ending at results and a short consequence epilogue.

| Target | Cap | Why |
|---|---:|---|
| First playthrough | **6–8 hours** | Long enough for routine and sacrifice to matter; short enough to polish |
| Replay content | **10–12 hours** | Replay comes from different lives and tower routes, not more geography |
| Simulated span | **21 latent days + tutorial day + exactly 10 countdown days + examination** | Three weeks demonstrate consistency and diminishing returns |
| Outdoor district | **0.10 km²** (~250 × 400 m) | Destinations 20–60 s apart. The old 0.35–0.5 km² target is too large |
| Locations | **6 district, 3 reusable home shells with 6 origin dressings, 1 tutorial map, 1 tower map** | Contrast without building a city |
| Named NPCs | **12** (6–8 central per run) | Depth over headcount |
| Ambient population | **16 visible, nonpersistent** | Implies population without simulating it |
| Player-facing systems | **8, hard limit** | Prevents life-simulator sprawl |
| Examination | **3 authored floors** | Each combining two disciplines |
| Combat | **One tool grammar, evade, contextual action, one derived ability** | Combat serves the premise; this is not a character-action game |

### The eight systems

1. Calendar, planning and activity scheduling
2. Latent growth, diminishing returns, Aptitude Projection
3. Condition: energy, hunger, health, stress
4. School and household obligations, small money costs
5. NPC schedules
6. Relationships and remembered events
7. Awakening, Tattva profile, one derived ability
8. Tower challenges, light combat, scoring, results

Saving, streaming, validation and input are infrastructure, not systems.

### Mandatory cuts

- **End at the examination.** No Class XII, university, adult professions, business ownership, housing progression, aging or legacy play. These are sequels, not scope.
- No open world, procedural city, vehicles, multiplayer, live service, crafting tree, loot tiers, repeatable Gate economy or systemic romance.
- Real Gate disasters stay as news, route closures and authored consequences — **not a second combat campaign.**
- Three modular home shells for six origins, not six bespoke houses.
- Fully 3D stylized characters from the first playable room. **Do not build a temporary Paper2D world-character pipeline.**
- Windows ships first. Android gets one early feasibility spike.
- No full voice acting. Text, limited barks, focused ambient audio.
- No MetaHuman, Lumen, Nanite or World Partition in the slice.

## 3. The vertical slice

**75–100 minutes, cooked Windows build, keyboard/mouse and controller.** It must contain the entire loop — ordinary life, awakening, tower, consequence — because a slice that omits any of those proves nothing.

Contents: the existing front end and creation flow; all six origins spawning through three greybox shells; three latent days (two school, one flexible); four ordinary activities (study, training, repair/help, social/organisational); fixed commitments, travel costs, four condition meters, daily diminishing returns, one missed-obligation consequence; five named roles; the Aptitude Projection shown before and after, visibly changing without exposing farmable numbers; the tutorial dungeon with its three conduct tests; awakening output with Tattva distribution and one derived ability; exactly ten countdown days with four preparation activities, three recurring obligations and two mutually exclusive opportunities; one authored world event as news plus a route consequence; one 12–15 minute tower floor with three legitimate routes; results mapping method and score to scholarship, licence and capital outcomes; and save checkpoints at four points.

**The gate:** at least 8 blind testers. **≥6 must explain their own awakening** and **≥6 must identify a meaningful obligation they sacrificed**, and each tower route must be independently discovered. Failing this gate means redesigning the projection and scoring — **not adding content.**

## 4. Architecture decisions to make now

Cheap today, catastrophic later.

| Decision | Recommendation | Cost of getting it wrong |
|---|---|---|
| **Time** | One monotonic `int64` game-minute clock. Exploration at 1 real second = 1 game minute. Activities and sleep jump through the same queue. Persistent mutations never originate from Actor Tick. | Every schedule, deadline and save needs rewriting; duplicated and skipped consequences |
| **State transitions** | Portable commands transform one validated state into another. Unreal Actors request commands and present results; they never own truth. | Rules spread across Blueprints and Tick become untestable |
| **World storage** | Versioned snapshots keyed by stable IDs. Actors are reconstructed projections. | Actor references and asset paths break on streaming, renames and schema change |
| **Content pipeline** | Portable JSON/CSV owns rules, schedules, effects, scoring. Unreal Data Assets own presentation and level assembly. Cross-layer validator. | Content becomes unauditable; renames silently corrupt saves |
| **NPC schedules** | Declarative time ranges, coarse zone IDs, travel durations, priority exceptions. Offscreen resolved only at event boundaries. | Continuous AI for every resident destroys CPU budget and determinism |
| **Determinism** | Separate persisted RNG streams for opportunities, tower variation and ambience. Stable queue ordering, command-replay hashes. | Reloads reroll history; failures cannot be reproduced |
| **Streaming** | One small exterior hub, no World Partition. Interiors as level instances. Tutorial and tower are separate maps. | Premature World Partition contaminates navigation, saves, lighting and packaging |
| **Characters** | Fully 3D stylized behind the existing display abstraction. Keep the engine-neutral appearance record. Low-poly placeholders until the rig exists. | A sprite pipeline becomes throwaway work |
| **Combat** | Spike GAS for combat state only. Keep time, school, relationships and money outside it. Prefer a small custom component unless the spike proves GAS earns its tax. | Premature GAS adds framework cost everywhere |

## 5. What AI agents can and cannot do here

**Well suited** — portable C++ reducers, state machines, serializers, migrations, validators, deterministic RNG, schedule resolution; unit, property, fuzz, migration and replay tests; narrow Unreal bridges, input plumbing, logging, build scripts, debug overlays; schema generation and stable-ID audits; greybox helper code; first drafts of dialogue and schedule tables.

These all have machine-observable feedback: the compiler, CTest, validators, UHT, a packaged launch.

**Poorly suited or impossible** — judging whether movement, camera, combat timing or time pressure *feel* good; final modelling, rigging, animation, lighting, VFX, sound, music, level composition; maintaining large binary Blueprint or level graphs; authentic contemporary Kolkata observation and Bengali nuance; responsible institutional-care portrayal; balancing hundreds of interactions from compiler output; blind usability and accessibility testing; maintaining character voice across thousands of lines; **and deciding what should not be built.**

**Operating rule:** one bounded change, one owner review, one objective check, one status update. Portable changes need CTest. Unreal interfaces need a UHT/editor build. Visual behaviour needs PIE. Milestones need a cooked build. Mobile claims need named-device evidence.

## 6. The five risks that kill this project

1. **Scope relapse into the AAAA fantasy.** Freeze the launch at age 16 through examination results. Maintain a written kill list. Every addition must remove equivalent effort.
2. **3D content throughput exceeds solo capacity.** Complete the full greybox loop before any bespoke art. One modular environment kit, one character rig, one animation library. Budget contractors for hero characters, animation cleanup, UI, audio and at least two Bengali cultural reviews.
3. **The life-to-awakening promise is not legible or fun.** Make the slice the sole priority and hold the blind-test gate. If it fails, redesign — do not add content.
4. **Simulation and saves become unreproducible.** One clock, one reducer, stable ordering, persisted RNG, record-based NPCs, checkpoint saves, migration tests, replay hashes.
5. **Tooling and QA load overwhelm one person.** Pin UE 5.7 and the compiler. Ship Windows. One Android spike. No MetaHuman, Lumen, Nanite or World Partition. No broad agent refactors. Regular cooked builds.

**On the existing test suite:** 225,000 assertions are evidence about core rules only. They say nothing about gameplay, packaging, art, performance or whether the game is any good. Do not let that number create false confidence.

## 7. Immediate build order

The next phase, in dependency order. Each step is one bounded deliverable with one objective pass condition.

| # | Deliverable | Pass condition |
|---|---|---|
| A | **Smoke-test everything already built** | Title, load, settings, appearance, household, points, save, resume all verified in a launched window. Five unverified steps is the project's largest current risk |
| B | **The clock** | Monotonic game-minute clock in the portable core, with activity jumps and sleep. CTest covers ordering, catch-up caps and save round trip |
| C | **Condition meters** | Energy, hunger, health, stress advancing against the clock with bounded effects. CTest covers recovery and that neglect cannot kill |
| D | **Activity and diminishing returns** | The rolling 24-hour practice window with the documented credit curve. CTest proves running in circles stops paying |
| E | **Aptitude Projection** | Latent stats produce a projected profession with a confidence band and named missing evidence. CTest proves it moves with behaviour |
| F | **World state and save v5** | Versioned world snapshot keyed by stable IDs, with migration. CTest covers round trip and corruption |
| G | **One greybox room, one pawn** | Walk, camera, collide, interact in one of the three shells, spawned from the origin record |
| H | **One school day end to end** | Wake, travel, two foreground classes, one activity, sleep — with the clock, conditions and projection all moving |

Steps B through F are pure portable C++ with tests and need no art, no editor and no artist. They are the ideal use of AI agents, and they are the foundation everything else attaches to.

**Step A comes first regardless.** Five consecutive features are built and unverified; that debt compounds.
