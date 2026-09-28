# The Unified Awakening and the Tower Examination

**Document status:** core design, established by the owner on 2026-09-20. This document is the authority on how awakening, the examination and profession freedom work. It replaced the old game-design document, which was deleted on 2026-09-28. Nothing here is implemented yet.

## The premise

Every student in India is awakened by the state on the same day: ten days before the All-India Unified Examination.

The stated reason is fairness. If awakening happened at random ages, the early-awakened would have years of advantage and the examination would measure nothing but luck. So the state performs it simultaneously, for everyone, through a supervised tutorial dungeon in every school. Ten days later they all sit the same examination: a climb of a virtual tower, with the results deciding scholarships, licences, capital and careers.

**The premise of the game is that this fairness is real and insufficient.** The ten days are identical for everyone. The sixteen years before them are not. A student who read every night, who ran, who repaired things, who argued and negotiated and helped, arrives at the tutorial dungeon carrying sixteen years of accumulated latent capacity. A student who did not, arrives with ten days.

The game never states this. It simply lets the player live those sixteen years' final stretch, then shows them what it bought.

## Three phases

| Phase | What the player does | Length |
|---|---|---|
| **Latent life** | Ordinary school life in Nabapara. No powers. Every meaningful action feeds latent stats. The projection terminal shows what they are becoming. | The bulk of the game *(note, 2026-09-23: under `GAME_PLAN.md` §4 the game is a full life, roughly 60–120 hours; the Class XI latent-life year is one phase of it, roughly 12–20 hours. "The bulk of the game" described this document's own three/four-phase scope, not the whole life that now follows the examination — see `GAME_PLAN.md` §4 Phases 6–10.)* |
| **The tutorial dungeon** | A supervised, sanctioned F-rank instance in the school. Completing it triggers awakening. Latent stats plus conduct inside decide the awakened profile. | One day |
| **The ten days** | A hard countdown. Awakened but untrained, with a fixed deadline and everything still to lose. | Exactly ten in-game days |
| **The examination** | The tower climb. Then the results, and the doors they open or close. | One event, then consequences |

The countdown is the spine. From the tutorial dungeon onward the player knows exactly how long they have, and every hour spent is an hour not spent elsewhere. Time pressure is the game's main source of drama, and it is honest pressure: the clock is public, identical for every student, and cannot be extended.

## Latent stats: the sixteen years

Before awakening there are no powers, no levels and no combat ability. There is only capacity, built the way a real teenager builds it.

| Action | Feeds |
|---|---|
| Reading, study, revision | Intelligence, Academics |
| Solving mathematics, puzzles, analysis | Analytical reasoning |
| Running, cycling, sport, the commute on foot | Agility, Stamina |
| Weight training, physical labour, carrying | Body, Strength |
| Repairing, building, taking things apart | Practical Repair, Technical Aptitude |
| Negotiating, arguing, helping, organising | Charisma, Communication |
| Sitting with discomfort, persisting, recovering | Willpower, Adversity Resilience |

These are not experience bars to grind. Each has diminishing returns within a day, so an hour of running matters and the sixth consecutive hour does not. What accumulates is consistency across many days, which is exactly the behaviour the game wants to reward: a life, not a session.

Creation-point traits set the starting position. They are a head start of roughly one week of ordinary progression, no more. A player who allocated nothing to study can out-read that gap in a fortnight of actually reading.

## The Aptitude Projection

At school there is a terminal — bureaucratic, unglamorous, queued for — that reads a student's latent stats and prints a projected awakened profession.

**Note, 2026-09-23.** "Prints a projected awakened profession" describes this document's original single-outcome framing. `PATHS_AND_PROGRESSION.md`'s legibility section supersedes it: the projection reports evidence per path, including which paths it has not yet observed enough to speak to, rather than collapsing to one most-likely profession. The confidence-band behaviour described below is unchanged; what changed is that the output is a per-path evidence picture, not a single named profession.

This is the game's most important interface, and it is deliberately imperfect:

- **It projects, it does not promise.** The projection shows a most-likely profession and a confidence band. A narrow band means the student's life points clearly one way. A wide band means they are still several people at once.
- **It updates as the player lives.** Read for a week and watch it drift. This makes the connection between ordinary action and eventual power legible without ever showing a raw number the player can farm.
- **It can be wrong.** Conduct inside the tutorial dungeon can move the outcome. A student projected as a researcher who spends the dungeon shielding classmates may awaken differently. The projection reads the sixteen years; the dungeon reads the person.
- **Queueing for it costs time.** It is not a menu. It is a place you go, which means a morning you did not spend studying.

This resolves the tension between legibility and optimisation. The player can steer. They cannot min-max a number, because the number is a summary of a life and the only way to move it is to live differently.

## The tutorial dungeon

Every school runs one. It is safe, supervised, F-rank, and the single most consequential hour of a student's life.

Design requirements:

- **It cannot be failed.** Every student awakens; that is the point of the policy. What varies is how.
- **It cannot be refused** (owner decision, 2026-09-25). A student who hangs back or withdraws to the supervised safe area still awakens, and that choice is read as conduct like any other. A student absent on the day is awakened at a supervised make-up session before the examination. Everything after the awakening, the examination included, stays optional.
- **It tests character, not capability.** The instance presents situations with no correct answer: a classmate who freezes, a shortcut that abandons someone, an objective that competes with a rescue. The player has no powers inside it. They can only observe, evade, assist and improvise.
- **It reads what the player actually did**, not what they said. This is the one moment where the hidden behavioural ledger surfaces and matters.
- **It is the same dungeon for everyone**, which makes it a fair test and a clean point of comparison between the player and named classmates.

Awakening does not grant a class. It grants a **profile**: a distribution across the five Tattvas, an initial profession projection, and the capacity to develop. Everything after is earned.

## The Tower Examination

The examination is a climb of a virtual tower, run by the state, entered simultaneously by every awakened student in the country.

**Virtual is not a technicality — it is the design solving a real problem.** Because the tower is a simulation, a student can be defeated without dying. Failure costs rank, not life. This lets the examination carry enormous stakes and genuine danger-feel while remaining a test rather than a slaughter, and it keeps the project's existing no-permadeath rule intact without making the climb feel weightless.

### Floors are not all combat

This is what makes profession freedom mechanically true rather than a promise in a document. The tower is a national examination for every future profession, so it tests every profession:

| Floor type | Tests | Rewards a player who |
|---|---|---|
| **Combat floors** | Reading tells, positioning, survival | trained physically, took risks |
| **Analysis floors** | Deduction, pattern, applied mathematics | studied, solved problems |
| **Endurance floors** | Sustained effort under degradation | ran, persisted, built stamina |
| **Construction floors** | Building, repairing, stabilising under pressure | fixed things, worked with their hands |
| **Negotiation floors** | Persuading NPC climbers, forming temporary alliances, resolving conflict | talked to people, helped people |
| **Logistics floors** | Allocating scarce supply across a team, trade, routing | organised, managed, planned |

No student clears every floor type well. **The climb is scored on how far you got and how you got there**, and a route that avoids your weaknesses is a legitimate strategy, not an exploit. A future businessperson climbs via logistics and negotiation and is graded highly. A future researcher climbs via analysis. A future hunter fights. A sportsman endures.

The tower is therefore the mechanism that delivers the owner's requirement: *business, academic, hunter, sportsman — all welcome, all real.*

### Rewards

Rank in the examination converts into the things that actually open doors in this society: scholarship and university placement, hunter licensing, starting capital and credit, sponsorship and patronage, equipment allowance, and public attention. These are the currencies of the post-examination game, and they are why the ten days matter.

## The world runs on its own

Separate from the player's story, the world has a fixed timeline of disasters: real Gate breaks, monster incursions, a tower malfunction, infrastructure failure. These are authored, scheduled, and happen whether or not the player is involved.

**They may or may not touch the player.** Some are distant news on the phone that establish that the world is larger than one student. Some close a road, cancel a school day, or put a named NPC in hospital. A few arrive on the player's street.

This is the contrast the game is built on:

- **The virtual tower is dangerous-feeling and safe.** You can lose everything that matters socially and lose nothing physically.
- **The real world is ordinary-feeling and lethal.** A real Gate break has no respawn, no rank, and no examiner.

A player who learns to fight in a simulation and then meets the real thing should feel the difference immediately and physically. That contrast is the game's best available source of genuine fear, and it should be protected: the real world must never be balanced like the tower.

## Failure

There is no losing the game. There is only a life that went differently.

- **The examination cannot be failed out of existence.** A poor rank closes prestigious doors and opens humbler ones. There is always a next thing.
- **Soft failures are real and cost time**, which is the only resource that never comes back: a failed subject, an injury that removes training days, a friendship strained past easy repair.
- **Real-world danger can genuinely harm the player**, up to hospitalisation and lasting consequence, but not permadeath. The existing rule stands.
- **World disasters are not the player's fault.** They are weather, not punishment. *(Refined, 2026-09-23: the scheduled **onset** of a hazard cannot be prevented — the fixed timeline in "The world runs on its own" above stands. Its **consequences** — damage, casualties, response effectiveness, fallout — are not fixed, and can be changed by preparation, expertise and the player's own action. The event happening is weather; how much it costs is not.)*

## What this changes in existing documents

These are corrections to be made, not open questions:

1. The old game-design document said the first Gate encounter is an accident and awakening emerges gradually and personally. It is now **universal, scheduled and state-administered**, via a school tutorial dungeon. The accidental-Gate opening arc must be rewritten.
2. The old game-design document described awakening paths as discovered at authored thresholds and never previewed. The **Aptitude Projection now previews them**, deliberately, with a confidence band.
3. The five-band lifecycle plan stands, but the **examination becomes the hinge** between Higher Secondary and everything after.
4. The `Tattva` system stands unchanged and now attaches to the awakened profile.
5. Time scale is confirmed unchanged: **one real minute is one game hour**, matching the existing one-real-second-to-one-game-minute rate.
6. Presentation is **fully 3D, stylized, not photoreal**; MetaHuman was considered and rejected on 2026-09-20 as stylistically wrong for the setting and too heavy for a large NPC cast with an eventual Android target.

## Open questions

Recorded so they are not silently decided by whoever implements first. **`GAME_PLAN.md` §12 is the master list of open owner questions.** This list keeps only the awakening- and examination-specific detail, and anything still open here is to be raised through §12. The biggest related question, whether the awakening itself can be refused, was `GAME_PLAN.md` §12 question A. **Answered, 2026-09-25: it cannot.** See "The tutorial dungeon" above.

- ~~Is the examination annual and repeatable, or once per student?~~ **Answered, 2026-09-23:** annual and repeatable, up to age twenty; the examination can also be deferred or declined, with apprenticeships and adult bridges as alternative qualification routes. See `GAME_PLAN.md` §1 (the freedom acceptance rule) and the ladder rules in `PATHS_AND_PROGRESSION.md`.
- How many in-game days of latent life precede the tutorial dungeon? *(Answered by `GAME_PLAN.md` §4 Phase 2: the Class XI year, roughly thirty-four weeks, Monday 2 June 2042 to Friday 23 January 2043.)*
- ~~Does the awakened profession harden at the examination, or stay fluid for life?~~ **Answered, 2026-09-23:** it does not harden into one profession. Results produce a portfolio of discipline credentials, scholarship and licence eligibility, and a public national rank — plural and revisable, not a single hardened outcome. See `GAME_PLAN.md` §4 Phase 5 and `PATHS_AND_PROGRESSION.md`.
- What happens socially and mechanically to students who rank at the very bottom? *(Partly answered, 2026-09-23: `GAME_PLAN.md` §4 Phase 5 requires that results always show actionable offers and "a usable future after a poor result". The specific social and institutional consequences are still open.)*
- Are there students who were awakened early, illegally or by accident, and is that a story? *(Partly answered: `GLOBAL_WORLD.md` §6 tension 3, "the awakening-age dispute", makes covert early awakening by some nations a live geopolitical scandal. Whether an individual early-awakened student appears as a character is still open.)*
