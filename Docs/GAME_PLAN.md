# Game plan

**Document status:** master plan, 2026-09-23, revised the same day after a GPT-6 (`gpt-6-astra`) review. This is the single entry point to the project. It defines the game, its full flow from boot to the end of a life, its presentation, its systems at overview level, its milestones, and how work gets done. Detail lives in the documents it points to.

**Authority, highest first:** the owner's recorded decisions (§10) → `VISION.md` (what must never be compromised) → this document → the specialist documents below. Where a specialist document conflicts with this one, this one wins and the specialist document is corrected. A referenced specification is either present and authoritative or explicitly marked unwritten, with an owner and a completion criterion.

**Nothing in this plan is implemented unless §11 says so.** The owner's instruction is to plan the full game and its workflow completely before building further.

| Document | Authority over |
|---|---|
| `VISION.md` | Why the game exists; what must never be compromised |
| **`GAME_PLAN.md`** | **This. What the game is, its full flow, time contract, milestones, decisions** |
| `SYSTEMS.md` + `SYSTEMS_CATALOGUE.md` | Architecture, command reducer, save, budgets, tests; and every system's data, commands, events and dependencies |
| `ASSET_PIPELINE.md` + `ASSET_INTEGRATION.md` | Block style, asset IDs, presentation contracts, resolver; and both import routes, AI-asset provenance, the M9 art validation |
| `DEVELOPMENT_WORKFLOW.md` | How the owner and AI agents build, verify and record work |
| `ROADMAP.md` + `ROADMAP_LATER.md` | Every milestone broken into bounded, verifiable steps (M0–M4, then M5–M10) |
| `ALTERNATE_WORLD.md` | Ordinary life, whole-life time, the world running without the player |
| `GLOBAL_WORLD.md` | Nations, Country Level, rankings, competitions, global tension |
| `PATHS_AND_PROGRESSION.md` | The ten paths and their ladders |
| `AWAKENING_AND_EXAM.md` | Awakening, tutorial dungeon, tower examination |
| `SCHOOL_AND_PROGRESSION.md` | Classroom, assessment, latent growth |
| `CHARACTER_CREATION.md` | The creation contract |
| `INDIA_WORLD_BIBLE.md` | Kolkata and West Bengal, cultural authority |

---

## 1. The game

**Perfect World: Awakening** is a first-person life simulation and role-playing game, switchable to third person, set in a fully alternate near-future Earth. The player begins at sixteen in Kolkata and may live an entire life, in any direction they choose, inside a world where Gates — rifts to hostile spaces — are a normalised hazard that thirty nations have organised their societies around.

**The one rule: the Gates are the setting, not the job.** A player may never fight anything and still have a complete, deep, dramatic game as a teacher, a clerk, a nurse, a shopkeeper, a parent.

**The thesis:** the state awakens every student on the same day so that the examination is fair. The ten days are equal. The sixteen years before them were not. The game never says this. It lets the player live it.

### Pillars

1. **Any life, a whole life.** Twenty-plus careers and ways of living, each with its own summit, from sixteen to old age. No path is the lesser path.
2. **Time is the currency.** A day holds only a few meaningful commitments. Every choice displaces another, and the game shows you what it displaced.
3. **The world measures everyone.** Individuals, universities and nations are publicly ranked. You always know where you stand, and the game never tells you how to feel about it.
4. **The world runs without you.** People have careers, families, ambitions and deaths whether or not you are watching. Hazards arrive on their own schedule; what they cost depends on who prepared.
5. **Power grows from living.** Awakened ability comes from who you have been. It is optional to practise, it never replaces ordinary life, and it cannot buy you the rest of the world.
6. **Dignity.** Poverty, institutional care, failure and ordinary work are depicted with respect. Nobody is set dressing.

### The freedom acceptance rule

**A complete life must remain available without combat, supernatural practice, competitive ambition, management, marriage or children.** Every supported choice leads to playable opportunities, never an empty calendar. The flow specifies participation, refusal, absence and withdrawal for the awakening and the examination, with their consequences and alternative qualification routes.

**Pending owner decision (§12, question A):** the recommended reading is that the awakening itself is universal and involuntary — it is the world's rule, and it happens to the player as to everyone — while everything after it is optional. The player may never use their ability, never enter a Gate, never fight. They may sit the examination by non-combat routes only, defer it to a later year (allowed to age twenty), or decline it and qualify through apprenticeships and adult bridges. Under this reading `VISION.md`'s "opt out of the supernatural entirely" means *opt out of supernatural practice*.

### What it is not

Not a power fantasy with a life-sim coat of paint. Not an open-world checklist. Not a management spreadsheet. Not multiplayer, not live-service, not pay-to-progress. There is no game-over; there is only a life that went differently.

---

## 2. Core concepts

Defined once here; detailed in the documents named.

| Concept | Meaning |
|---|---|
| **Gate** | A rift to a hostile space. Ranked F (least) to S (frontier). Society contains, insures, studies and exploits them |
| **Hazard onset / consequence** | A scheduled hazard's *onset* cannot be prevented. Its *consequences* — damage, casualties, response quality, fallout — depend on preparation and expertise, including the player's. Every major outcome records its causes, the parties affected, and the player's contribution |
| **Unified Awakening** | Every nation awakens its sixteen-year-old cohort on the same day, ten days before the national examination |
| **Tutorial dungeon** | The supervised F-rank instance at every school through which awakening happens. Cannot be failed; reads conduct, not claims. Outcomes are differentiated, never morally ranked |
| **Tower Examination** | The national examination: a climb of a government-run *virtual* tower. Defeat costs rank, not life. Floors test combat, analysis, endurance, construction, negotiation and logistics |
| **Latent stats** | Attributes (Body, Agility, Intelligence, Perception, Willpower, Charisma) and skills, built by ordinary life throughout |
| **Tattvas** | Prithvi (structure), Jala (adaptation), Agni (transformation), Vayu (motion), Akasha (space and perception). Affinity changes method, never ceiling. An employer may restrict a method, but every summit stays reachable by every affinity |
| **Awakened profile** | A Tattva distribution, a capability profile, one derived ability. Not a class |
| **Aptitude Projection** | The school terminal reporting evidence per path, including what it has not observed |
| **Path** | A way of life with a ladder. Ten world-summit paths plus the ordinary careers |
| **Four records** | Mastered methods, professional permissions, demonstrated achievements, relationships. A career is how they are currently organised, not a slot |
| **Interaction layers** | Direct control · contextual interaction · four-structure problems · routine resolution (§5) |
| **Interaction structures** | Construct an explanation, model a system, investigate a fault, coordinate a group. The verbs of all knowledge work |
| **Problem family** | An authored set of related problems in one structure and one domain |
| **Condition** | Energy, hunger, health, stress. Bounded effects; ordinary neglect never kills |
| **Opportunity receipt** | What a commitment displaced — only alternatives the player knew about |
| **Thread** | A persistent unresolved tension that escalates on a clock. The story engine |
| **Country Level** | A nation's public 1–100 composite. Each of the ten paths contributes exactly 10%. Inputs update continuously, it is **published quarterly**, and reviewed annually |
| **World Convergence** | The annual world championships: ten disciplines, sixty entries each |
| **Persistent identity** | Every person gets a stable identity at their **first consequential reference** — named in dialogue, news, rankings, correspondence, family history, or any record the player can see. Identity and history never change afterward |
| **Simulation fidelity** | How closely a person is currently simulated: active local cast, significant, cohort, population. Fidelity rises and falls; identity does not |
| **Crystallisation** | A cohort member with no identity yet is generated consistent with their cohort and the world's event log at first consequential reference, then persists |
| **Resume point** | Where the player was when a slot was written |
| **Asset ID** | The stable name of every visual and sound. Gameplay references IDs, never files |

---

## 3. Presentation

### Voxel blocks for the whole of development

Every visual is built from blocks — cuboids with low-resolution pixel textures — until the game is complete. Real art arrives at M9. The block look is the game's working identity for years, so it is designed, not improvised.

1. **Voxel is a look, not an engine.** Ordinary Unreal static and instanced meshes with nearest-filtered pixel textures. No voxel terrain, destruction or plugin, so real FBX and GLB art drops into the standard pipeline.
2. **Real metric scale.** A door is 2.1 m tall whatever it is made of. Gameplay never depends on the block grid.
3. **Gameplay data lives in data, not meshes.** Collision, interaction anchors, seats, sockets and navigation belong to the asset ID.
4. **Characters use the final skeleton from day one.** Block characters are cuboid parts on the standard Unreal humanoid skeleton. Skeleton version and proportions policy are frozen early.
5. **Presentation contracts, tested in blocks.** Every asset family has a versioned contract: dimensions, the permitted visual envelope, collision, interaction anchors, camera clearance in both modes, seated clearance and reach, pose requirements, semantic cues (what makes it recognisable at reading distance) and a performance allocation. Contracts are exercised with alternate block constructions throughout development. **Stable IDs preserve references; contracts preserve perception.**
6. **Performance budgets from M1.** Target hardware, frame, memory, streaming and draw-call budgets, with explicit headroom for final art, exercised with synthetic block loads.

`ASSET_PIPELINE.md` has the style guide, the generator, the contracts and both import routes.

### Camera: first person, switchable to third

- **First person by default** for walking, interacting, working, studying and fighting. **Third person available at any time** — it is how the player sees themselves, so it is fully supported, with reliable orbit and collision.
- **Authored framing is optional or immediately releasable.** The appearance editor uses an orbit camera; nothing forces a camera the player cannot leave.
- **Both modes share one set of reach, targeting and interaction rules.** Every activity works in both; neither becomes a separate balance system.
- **Comfort from M1:** field-of-view and sensitivity sliders, no cosmetic head motion on the first-person camera, stable horizon, reduced shake, motion-blur and roll controls, adjustable third-person distance, subtitles.

### First person in a life simulation

The camera puts the player close enough to notice everything blocks omit. The plan commits to:

- **Faces and identity:** a block-face expression vocabulary (gaze, blink, brow, mouth, head angle, posture). Recurring people have distinct silhouettes, clothing and movement, and are recognisable without nameplates.
- **Multi-channel meaning:** consequential information never depends only on a small facial change, colour, precise aiming or forced camera movement.
- **Conversations:** authored positions, speaker identification, optional conversational framing, readable off-screen speech.
- **Hands and contact:** grips, reach envelopes, seated poses, held-item sockets. Tested on writing, eating, carrying, repairing and helping, not just walking.
- **Targeting:** forgiving selection with predictable priority among overlapping interactables. Everyday actions never require shooter dexterity.
- **Seeing yourself:** third person, milestone photographs, portraits and wardrobe previews. Mirrors supplement; they are never required.
- **Spaces:** distinct massing, landmarks, lighting and sound per space; route previews and room labels; an accessible navigation option. A block interior test suite — home, classroom, shop, ward, office — at realistic furniture density.

### Interface

The phone is the primary interface and exists in the world: calendar, planner, receipts, news, rankings, messages, career folio, maps. **It opens as a flat, screen-aligned, scalable view** fully navigable by keyboard and controller — no perspective-distorted text, no repeated opening animation, no aiming at messages. Full-screen menus serve the front end, creator, settings and pause. A **presentation bible** governs how injury, poverty, institutional care, grief, intimacy and violence are shown, because detailed art can make cruel what blocks made tolerable.

---

## 4. The full game flow

**Phase completeness rule:** every phase specifies entry conditions, a representative week, sources of satisfaction, major commitments, voluntary exits, failure recovery and transition scenes. Age bands describe common pacing, not permission gates. Stable and noncompetitive lives get equal narrative attention.

### Phase 0 — Front end

Title: **Continue**, **New Game**, **Load Game**, **Settings**, **Credits**, **Exit**. Slots show character, age, date, location, current commitments and last action. Life slots are distinct from recovery checkpoints. Returning after a long absence offers an optional **"since you last played"** recap. *Built:* title, load, settings, three slots, resume point, migrating saves. *Not built:* Continue, Credits, slot detail, recovery checkpoints, recap.

### Phase 1 — New Life (creation)

Under **eight minutes with a preset**, twenty to thirty from scratch; drafts preserved. Appearance (block character, orbit camera), identity, household and resources (rank 0 is a children's home), school stream, 1,000 trait points, review. The review previews a representative day and concrete trade-offs, explaining choices through likely circumstances rather than future builds. *Built:* appearance, household, traits, confirm. *Not built:* identity, stream, review, presets beyond one.

### Phase 2 — Latent life: the Class XI year (~age 16, 12–20 hours)

**Monday 2 June 2042, 06:00** to **Friday 23 January 2043** — about thirty-four weeks. The player wakes in their origin home in first person and lives the year: school (at most two foreground classes a day), household, growth under the rolling practice window, the projection, relationships and threads. The calendar is real — monsoon, Independence Day, Durga Puja, Kali Puja and Diwali, winter, Saraswati Puja — with dates from the actual 2042–43 almanac, verified by a cultural reviewer.

**This year is a complete game, not a waiting room.** It delivers finished ordinary-life arcs before the awakening: a friendship that changes, a practical project that matters, a household obligation redesigned. Goals and relationships shift across the year, not just the projection.

### Phase 3 — Awakening Day (1–2 hours)

**Friday 23 January 2043.** The tutorial dungeon with the player's own classmates, carrying existing relationships through. Three conduct tests whose consequences are differentiated, not ranked. The awakening explains what conduct contributed without passing a verdict on the player's personality. Refusal, absence and withdrawal are supported per §1.

### Phase 4 — The ten days (2–4 hours)

From the evening of Awakening Day to 06:00 on examination morning. Opportunities shaped by earlier relationships, uncertain but disclosed outcomes, negotiable obligations, one world event arriving as news and a route consequence. **Several preparation strategies must be defensible**, so it never reads as a single optimal worksheet.

### Phase 5 — The Tower Examination (1–2 hours)

**Monday 2 February 2043.** A route matrix guarantees every discipline and every non-combat path substantive coverage. Accessible timing; interruption-safe saves mid-climb. Results show actionable offers and alternatives, including **a usable future after a poor result**.

### Phase 6 — The rest of school (16–18, 6–10 hours)

The biggest pacing risk: returning to school after the climax. So it is **a new stage** — changed status, permissions, peer standing, mentor access, practical responsibility. Board examinations are distinct from the tower. Applications, funding, qualification gaps and household negotiation resolve before adulthood.

### Phase 7 — Young adulthood (18–22, 10–20 hours)

Every route — university, apprenticeship, a job, a hunter licence, a combination — has a **first week, a first consequential task, a representative routine and a recovery route**. Leaving care, staying home, deposits, transport, income gaps and changing commitments are all defined.

### Phase 8 — Career (22–35, 15–25 hours)

Repetition is fought by **changing the player's relationship to problems**, not just the problems: the same repair means learning at sixteen, protecting a colleague at thirty, revisiting your own bad design at forty, and teaching an apprentice at sixty. Long projects with visible outcomes, career switches, setbacks, local goals independent of championships. A deliberately stable career is valid.

### Phase 9 — Established life (35–55, 10–15 hours)

Rich non-managerial lives: expert practice, neighbourhood work, friendship, craft, care, second careers. Mentoring colleagues and outsiders, not only children. Recurring places and people persist while new generations arrive.

### Phase 10 — Later life (55+, 5–10 hours)

New projects, learning and relationships remain available; later life is not preparation for disappearance. Closure has its own playable interval. **The life ends** by the player's choice or by death in old age, whose timing is signalled enough that final intentions are not lost arbitrarily. **The epilogue is plural:** people, places, work, ordinary memories, unresolved matters — and, optionally, ranking history.

**Total: 60–120 hours for a representative complete life** — a target envelope to be reconciled against measured runs (§7), neither a minimum to grind through nor a cap on players who prefer daily play.

---

## 5. Time contract and core loops

**One clock, several modes.** Every interaction declares its clock mode, game-time cost, interruption policy and resolution boundary.

| Mode | Clock behaviour |
|---|---|
| **Free roam** | One game minute per real second |
| **Conversation** | Paused, or charged a declared cost per exchange — a three-minute talk never silently costs three hours |
| **Scene / activity** | Declared game-time cost, charged at resolution |
| **Phone, menus, reading** | Paused |
| **Accessibility pause** | Paused; reading and accessibility adjustments never consume opportunity time |
| **Mission** | The tower and Gates run a separate mission clock; the outside world advances at a capped rate |

**Equivalence rule:** the same plan resolved as scenes, weeks, seasons or years produces the same persistent outcome unless the player decides differently. Compression discloses any consequential choice made on the player's behalf.

### Interaction layers

| Layer | Examples | Owned by |
|---|---|---|
| **Direct control** | Movement, sport execution, combat, hands-on manipulation | Player controller and activity systems |
| **Contextual interaction** | Sit, pick up, use, open, talk | Interaction system |
| **Four-structure problems** | A class, a shift task, a diagnosis, a negotiation | Activities and problem families |
| **Routine resolution** | Mastered work resolved at full value | Calendar and time granularity |

An athlete trains and competes through direct control. No path is an allocation screen with its fiction painted on.

### Loops

| Loop | Duration | What happens |
|---|---|---|
| **Moment** | seconds | First-person free roam: walk, look, interact, talk, open the phone |
| **Scene** | 3–15 min | One interaction applied to one problem |
| **Day** | 5–40 min | Morning plan → commitments → free blocks → evening → sleep → receipt |
| **Week** | 2–5 min as routine | Plan, resolve, at most one interrupt, weekly receipt; up to three nominated scene days |
| **Season** | 1–2 min | Rankings and Country Level publish, events fire, relationships drift, ageing applies |
| **Year** | minutes | The Convergence, examinations, promotions, birthdays, Country Level annual review |

**Repetition is measured on three axes:** has the player already solved this decision structure, occupied this social role, seen this consequence? Compression removes repeated execution while preserving chosen involvement. A pleasant ordinary day must be playable without a crisis to justify it.

---

## 6. Systems overview

Specified in `SYSTEMS.md`.

| Group | Systems |
|---|---|
| **Foundation** | Game clock *(built)* · time-mode controller · command reducer · deterministic RNG streams · persistent identity registry · world state and versioned save *(creation save built)* · content registry and validator · asset registry and resolver |
| **Character** | Creation *(built)* · attributes, skills, latent growth · condition · awakening profile and abilities · appearance *(record built)* |
| **Life** | Activities, interaction structures, problem families · school · work and careers · **economy and ownership** (inventory, tools, contracts, debt, insurance, household budgets, enterprises, employees) · household and housing · **relationships and households** (friendship, shared finances, care, cohabitation, estrangement, bereavement) · **life events and recovery** (unemployment, dismissal, insolvency, retraining, retirement) · threads · receipts · projection · calendar |
| **World** | Locations and interiors · NPC fidelity tiers and crystallisation · schedules · hazards, onset and consequence · news · **a changing world** (new generations, evolving institutions, business turnover, technology adoption) |
| **Global** | Nations and Country Level · rankings · competitions and the Convergence · geopolitical tensions |
| **Supernatural** | Gates · tutorial dungeon · tower · combat |
| **Presentation** | First/third-person camera · player controller · interaction and targeting · phone view · block renderer and asset resolver · audio · accessibility and comfort · front end *(built)* |

**Binding architecture rules:** one clock; portable commands transform validated state and Unreal actors only present it; versioned snapshots keyed by stable IDs; content as validated data; declarative NPC schedules resolved at event boundaries; separated, persisted RNG streams; no persistent mutation from a frame tick.

---

## 7. Content and duration accounting

| Content | Target |
|---|---|
| Playable city | Nabapara and surrounding Kolkata districts; further cities in later milestones; foreign locations only where the story needs the player's body there |
| Home presentation | 3 family shells × 5 templates × **4** family resource tiers, plus the children's-home dormitory |
| People | ~50 active local cast · ~500 significant · ~240 persistent global notables (3–8 per nation) · cohorts · aggregates |
| Nations simulated | 30 |
| Paths | 10 world-summit paths + ~13 ordinary careers |
| Problem families | ~40 at the vertical slice, ~300 for the full game |
| Authored world events | ~60 across a full life |
| Thread templates | ~120 |

Two living artefacts are maintained from M2 onward:

- **The content matrix** links each path and phase to its decisions, social roles, relationship arcs, problem families, recovery routes and endings. Counts of families and scenes are not evidence of variety; the matrix is.
- **The playtime budget** separates scenes, routine resolution, travel, reading and administration. The 60–120 hour target is reconciled against **measured** complete-life runs. School through board results is currently budgeted at 22–38 hours, a large share of a life; that is acceptable only if early relationships and events keep their significance for decades.

---

## 8. Milestones

Every milestone is playable and ends with a gate. `ROADMAP.md` breaks each into steps. **Early lifecycle proof:** adulthood is probed before the school year expands, and a skeletal complete life exists before breadth is built. **Repetition gate:** content expansion requires evidence that each interaction structure survives forty exposures with routine resolution available.

| # | Milestone | Gate |
|---|---|---|
| **M0** | Foundation | Every built screen verified in a launched window, including damaged saves, controller focus, settings persistence and recovery. First cooked Windows build |
| **M1** | Walkable block world: controller in both cameras, one home, a street, the school gate; asset registry, placeholder generator, presentation contracts, performance budgets, comfort options | In **both** camera modes: sit, read, select a small item, use a tool, speak to someone, pass through a furnished room, switch camera |
| **M2** | One school day: condition, activities, all four structures prototyped, two classmates, a teacher, schedules, receipts, persistent identity | A full day saved and resumed mid-day. **The forty-exposure classroom test** passes with abbreviation available |
| **M3** | The Class XI year: time modes, week compression, calendar, projection, threads, all six origins, a lightweight thirty-nation data model feeding news and rankings | A short period played manually and compressed yields equivalent state and understanding, **then** thirty-four weeks. **Adult-task probe:** one established adult work week built on the shared structures |
| **M4** | Awakening and examination | Vertical slice. ≥6 of 8 blind testers explain their own awakening and a sacrificed obligation; each tower route is independently discovered; non-combat routes are substantive; a poor result leads somewhere worth playing |
| **M5** | Adult life, plus a **skeletal later life and epilogue** | A non-hunter lives 16 → 30 and holds up; an established worker's week holds up; a whole life runs end to end |
| **M6** | The living nation | Demotion, re-encounter, cohort replacement and causal attribution hold across decades |
| **M7** | The world | The player's standing moves India's rating understandably — and a player with negligible influence still feels the world |
| **M8** | Breadth | Complete lives with fresh testers across the scenario matrix: stable ordinary life, homemaking and care, late career switch, hybrid career, summit pursuit, recovery after serious failure |
| **M9** | Art integration | Three separate tests: asset coverage and provenance; no unintended change to rules, saves, collision contracts or outcomes; final-presentation acceptance for readability, contact, comfort, performance, accessibility and tone. Includes a scheduled presentation-adjustment period |
| **M10** | Ship | Final certification. Performance, accessibility, localisation support and cooked builds began at their earlier milestones |

**Playtest findings are labelled** by what they establish — rules comprehension, pacing, block readability, or final presentation — so block-era comfort and emotional findings are retested at M9 rather than assumed.

---

## 9. How work gets done

`DEVELOPMENT_WORKFLOW.md` is authoritative. In brief: the owner decides, plays and tests; agents implement, test and document. One bounded change, one objective check, one status update. Verification ladder: CTest → editor build → Play In Editor → cooked build; never claim a rung that was not run. Claude implements in the repository; GPT-6 (`gpt-6-astra`) critiques design; GPT-5.6 Sol handles routine implementation through Codex. On a quota limit, preserve partial work and report it. No `Co-Authored-By` trailers.

**Durable production contract:** every delivery records implemented behaviour, verification evidence, unresolved limits and resumable next work. Content requires schema validation, reachable-state preview, continuity review and representative human playtesting. **AI generates content faster than one owner can verify meaning**, so the acceptance workflow — not generation — is the planned bottleneck, and content volume is paced to it. Migration fixtures, deterministic repro cases, asset provenance and handoff records are preserved throughout.

---

## 10. Decision log

| Date | Decision |
|---|---|
| 2026-09-19 | Kolkata, fictional Nabapara and Nabapara Vidyalaya; age exactly 16, Class XI; 1,000 creation points across twenty traits; Unreal 5.7, C++ |
| 2026-09-20 | Five household templates; three JSON save slots, `USaveGame` wrapper deferred |
| 2026-09-20 | Household Resources rank 0 means growing up in institutional care |
| 2026-09-20 | Fully 3D; stylised final art, **not** MetaHuman; Windows first, Android eventually |
| 2026-09-20 | Free-roam world; one real minute is one game hour |
| 2026-09-20 | Unified Awakening ten days before a virtual-tower national examination, through a school tutorial dungeon |
| 2026-09-20 | World-level scope: thirty nations, national and world competitions, rankings, Country Level |
| 2026-09-20 | Full life freedom; the supernatural is optional; plan at full scope and build in layers while tooling improves |
| 2026-09-20 | Appearance: colour sliders plus discrete part pickers, stored as engine-neutral data |
| 2026-09-23 | **Voxel block visuals for the whole of development**; real art integrated only after the game is complete |
| 2026-09-23 | **First-person camera by default, switchable to third person** |
| 2026-09-23 | **Real art from Blender/FBX/GLB and AI-generated sources**, through both a drop folder and editor import |
| 2026-09-23 | No `Co-Authored-By` trailers on commits |
| 2026-09-25 | **Upgrade from Unreal Engine 5.7 to 5.8** to use Epic's official, in-editor Unreal MCP server (experimental, localhost only). Migrate now, before any world content exists, while it is cheapest. `ROADMAP.md` M0-12a to M0-12c |
| 2026-09-23 | GPT-6 review adopted: time contract, persistent identity at first reference, hazard onset versus consequence, presentation contracts, M9 as a three-part validation, early lifecycle proof, repetition gate, phase completeness rule, freedom acceptance rule |

---

## 11. What exists today

Built, compiling and covered by the portable test suite (225,374 creation assertions, 16,857 clock assertions), pushed to `debdutta777/Perfect-World`: character-creation rules (ledger, households, rank-0 care, opening-home plan); an engine-neutral appearance record; three JSON save slots at schema version 4 with migration from 1–3 and resume points; the game clock; the front end (title, load, settings, appearance editor, household and points panel).

**Not one screen has been exercised in a launched window.** Six features are built and unverified; closing that is the first step of M0 and of all work after this plan. No world, room, character model, NPC, activity or gameplay exists yet. The CC0 furniture packs in `Content/Fab/Downloads/` are superseded by the block approach.

---

## 12. Open questions for the owner

**A. Supernatural participation.** Is the awakening universal and involuntary, with everything after it optional (recommended; §1)? Or may a student legally refuse awakening itself?

**B. The thirty nations.** Recommended: real countries as alternate-world versions, with every institution, organisation, company, person, power bloc and incident fictional, and no real-world conflict depicted directly. The alternative is fictional states on real geography. This governs naming, maps, cultural review and geopolitical writing.

**C. Legacy play.** When the character's life ends, may the player continue as a child or successor?

**D. Adult relationships.** Romance, marriage and children for adult characters — in scope, and at what depth? Adult/minor romance and explicit content remain excluded. The rest of the family, care and household foundation is specified regardless (§6).

**E. Content rating.** Teen or mature? This decides how the criminal path, violence and Gate casualties are shown.

**F. Starting cities.** Kolkata only, or eventually a choice of Indian cities or other nations?

**G. Language.** English interface with Bengali and Hindi in the world, or full localisation — and when?

**H. Business model.** A one-time premium purchase is assumed; no paid progression under any model.
