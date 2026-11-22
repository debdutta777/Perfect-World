# The compressed year: the R6 slice start

**Status: PROPOSAL, pending owner acceptance.** Nothing here is canon or implemented until the owner accepts it. Written 2026-11-22 by Claude Sonnet (game design). It designs what the owner decided in `GAME_PLAN.md:§10` (row 2026-11-22, point 5) and `GAME_PLAN.md:§12 M` (R6 answered): the blind-test slice starts with **a compressed year**, a fast-forward of Class XI with a few key choices that leads into Awakening Day, **not a prepared late-year save**. Testers must still explain their own awakening and name a sacrificed obligation (`ROADMAP.md:§8 M4-20`; `GAME_PLAN.md:§8 M4`).

Source citations below use `Core/P/` for `Source/PerfectWorldCore/Public/`. These files are absent from the `w-r6` sparse worktree and were read in the main clone; verify against the integration branch before implementing.

## 0. The idea in one paragraph

The tester creates a 16-year-old (preset allowed, `GAME_PLAN.md:§4 Phase 1`), plays a short real lead-in, then lives the 34 weeks to Awakening Day through the **existing week compression**: a plan resolved in bulk, with every consequential choice either a standing order the tester gave or a stop for play. Five authored decision points are real recorded decisions with disclosed trade-offs. Nothing is scripted and no history is invented: the core resolves each week from the tester's own orders, so the Awakening they receive is computed from what their year actually contained. The slice's first checkpoint, awakening morning, is the arrival of that run.

## 1. Where it starts, and the playable lead-in

**Start.** Character creation confirmed at exactly 16, Class XI, Kolkata, Nabapara Vidyalaya (`GAME_PLAN.md:§10` 2026-09-19). The clock opens at **Monday 2 June 2042, 06:00** (`GAME_PLAN.md:§4 Phase 2`). The awakening state is seeded once on creation confirmation and never reseeded on load or compression (`Core/P/AwakeningInputs.h:header contract`). The opening-home plan and rank-0 care home already exist (`GAME_PLAN.md:§11`).

**Playable lead-in (target 15 to 20 real minutes, proposed).** Free roam, first person, one real day:
1. Wake at home (family) or the care home (rank 0); a household or care-home morning errand.
2. Walk to school; at most two foreground lessons (`Core/P/SchoolSessions.h:SchoolForegroundLimit`), the rest Attend, because the first foreground use of a structure is a "first of kind" that never auto-resolves (`Core/P/WeekCompression.h:FirstOfKind`).
3. One conversation with a named classmate and one with a teacher, so the cast who will later stand in the tutorial dungeon exist as identities (`GAME_PLAN.md:§2` persistent identity).
4. Evening: set the **first standing orders** (section 3, decision 1) at the phone, with receipts visible. The tester sees what an order is before any week is compressed on their behalf.

Rationale: the lead-in teaches the vocabulary the rest of the slice relies on (order, receipt, displaced alternative) in one day; it satisfies the equivalence rule's spirit that compression removes repeated execution, not first experiences (`GAME_PLAN.md:§5` loops).

## 2. How the year is compressed honestly

### 2.1 Mechanism: existing, no new resolver

- A span is resolved week by week through `CompressSchoolWeek`: ordinary entries go through `CompressWeek`, each lesson through the school-sessions and day-loop owners (`Core/P/SchoolWeek.h:CompressSchoolWeek`). A week holds at most 7 days and at most **one interrupt** (`Core/P/WeekCompression.h:WeekMaxDays, WeekMaxInterrupts`). Year compression as a single call is M5 scope and is **not** assumed (`SYSTEMS.md:§2.3`, §8 budgets name it; `ROADMAP_LATER.md` M5 owns it). The slice chains about 34 week calls under a driver, each inside the 150 ms week budget (`SYSTEMS.md:§8`).
- Standing orders are the only way compression decides: `school.week.order`, no two covering one weekday (`Core/P/SchoolWeek.h:SetSchoolStandingOrder`). Every order-decided entry is listed as `decided_by_order` on the receipt (`SYSTEMS.md:§2.5 rule 3`; `Core/P/WeekCompression.h:DecidedByOrder`).
- A first of kind, or a consequential choice no order covers, stops compression and becomes play or a decision point (`Core/P/WeekCompression.h:WeekStop::FirstNeedsPlay, InterruptBudgetSpent`). A stopped result is a success and the world is exactly the step-by-step state (`Core/P/WeekCompression.h:DISCLOSURE`).
- Equivalence is an existing CTest: compressed and step-by-step weeks match field by field except command counts and receipt text (`STATUS.md:Checkpoint M3-02`; `GAME_PLAN.md:§5` equivalence rule). The year driver adds no rule outside this path, so the property carries over.

### 2.2 What advances, and who computes it

| System | How it advances during compression | Owner of the result |
|---|---|---|
| **Lessons** | Bells expanded into arrival plus lesson entries; Foreground, Routine (needs mastered family) or Attend; attendance is not evidence of mastery | `SchoolWeek.h`, `SchoolSessions.h` (header contract: "attendance is NOT evidence") |
| **Latent growth** | Completed activity events credited once each, after the rolling practice window; keyed per-day RNG | `AwakeningInputs.h` (credits reducer), `PracticeWindow.h` |
| **Homework and assessments** | Briefs issued by calendar (`school_work_brief`, 27 across three terms and three streams); submitted, late or lapsed by what the tester's plan covered; recovery stays open after the due date | `SchoolWork.h:IssueSchoolWork` |
| **Household obligations** | The evening rota window (18:00 to 22:00) is a plan block; kept by default, redesigned through `p2.household_responsibility` (family or care-home variant) | `SchoolLifeContent.h`, `QuestLog.h` |
| **Relationships and threads** | Scene choices apply relationship acts and thread events; drift runs by the season rules | `SchoolLife.h:ChooseSchoolScene`, `Threads.h` |
| **Aptitude projection** | Receives only evidence a system actually observed; unobserved stays "unknown" | `AwakeningInputs.h`, `AptitudeProjection.h` |
| **Calendar** | Real 2042-43 almanac: puja vacation, Diwali, winter break, unit tests, half-yearly, sports day | `calendar.nabapara_2042_43.json` |

None of this is scripted. The only authored content is the catalogue (scenes, briefs, bells, calendar) the tester's plan meets.

### 2.3 What compression must never do

- Invent a lesson, friendship, grade or favour the plan did not produce. Weeks the tester did not order resolve to the **default week** (attend, kept household rota, ordinary sleep), disclosed as "default", never as an inferred preference.
- Decide a decision point. Each of the five stops the run through the weekly interrupt (section 3).
- Skip the calendar: an unavoidable event (monsoon, a holiday, a unit test falling due) lands on its real day.

### 2.4 Honest limit, and the precise claim about "conduct evidence"

`ConductLedger` is written **only** by the three tutorial-dungeon tests (`Core/P/ConductLedger.h:header`; `AwakeningInputs.h`: "R9-02 owns tutorial orchestration"). The year therefore feeds the Awakening through **Stats, observed Projection evidence, relationships and thread state**, plus the people present in the dungeon. It does not write conduct. This proposal keeps that: the year shapes who the tester is and who stands beside them; the dungeon reads what they then do (`AWAKENING_AND_EXAM.md:The tutorial dungeon`, "the projection reads the sixteen years; the dungeon reads the person"). The M4-20 test is that the tester can trace both, via `AwakeningCause` sources (`attribute.*`, `skill.*`, `facet.*`, `conduct.*`, with test and choice ids; `Core/P/Awakening.h:AwakeningCause`). **Open question Q3** asks whether the owner wants anything stronger.

## 3. The five key choices (a sixth optional)

Each is a stop in a chosen week, a real command recorded in the session log (`ROADMAP.md:§8 M4-17`), and shows its **displaced alternatives before commit** (opportunity receipts; `SCHOOL_AND_PROGRESSION.md:Ten innovations #1`). Each has a refusal that is not a penalty (`SchoolLife.h:IsRefusal`; `GAME_PLAN.md:§1` dignity). Scene ids are existing `Data/Content/school_scene/`.

| # | Week (approx.) | Decision | Trade-off disclosed | Records written |
|---|---|---|---|---|
| **1** | 1 (end of lead-in) | **The evening order.** Set the weekday evening block: study window, the household or care-home rota in full, or a split. | Study hours versus a person's workload (`SCHOOL_AND_PROGRESSION.md:Ten innovations #7`, obligations have an outcome not an animation) | Standing order; rota thread opened (`thread.evening_rota`) |
| **2** | ~6, before `unit_test_1` | **The rota redesign** (`p2.household_responsibility`): keep it, batch it, swap it, or release it to study. Rank 0: negotiate with the care worker (07:00 to 15:00 duty). | Time released for study versus the adult's burden and goodwill | Relationship act with `person.household_adult` or `person.care_worker`; thread resolution |
| **3** | ~14 (term 2) | **The friendship** (`p2.changing_friendship`): stay with the classmate under afternoon pressure, or take the new circle. Plus `p2.football_or_coaching` as the contested afternoon. | One afternoon, one person; whichever is chosen, the other is shown as missed | Relationship acts both ways; thread events |
| **4** | ~22 (before `half_yearly`) | **The paper**: do the assigned work properly, submit late and say so, or let it lapse (`p2.copied_homework` adds the shortcut). Recovery stays open (`SchoolWork.h`). | Marks and standing versus a night's rest or a sibling's evening | Submission, lateness and diagnostic marks recorded separately |
| **5** | ~29 (before `ten_days` window) | **The practice order**: one standing practice family for the last months (written study, endurance, repair, community work, explanation of others). | The rolling window caps any single family (`PracticeWindow.h`), so a narrower year means a narrower projection, with unobserved paths named, never hidden | Standing order; projection evidence |
| **6 (optional)** | ~33, last week | **The projection terminal**: queue a morning to read it, or do not. | A morning not spent elsewhere (`AWAKENING_AND_EXAM.md:The Aptitude Projection`) | A visit (or its absence) |

**Guarantees.** (a) At least one **obligation is sacrificed by the tester's own choice**: decisions 1, 2 and 4 each have a branch that sets something real aside (a rota block, a person's request, a homework paper), and no branch of 1, 2 and 4 keeps all three at full cost because the weekly time budget forbids it (`GAME_PLAN.md:§1` pillar 2: every choice displaces another; `SYSTEMS.md:§2.5`). (b) **The defaults also sacrifice.** A tester who chooses nothing still defaults to attendance and the kept rota, which displaces study and practice; the receipt says so. A sacrificed obligation is therefore always derivable from their records, even for the most passive run. (c) The slice gate counts the tester **naming** it, not the game naming it for them (`ROADMAP.md:§8 M4-20`).

**Obligation record (to verify, then maybe build).** The existing sources are opportunity receipts, thread resolutions, relationship acts and school-work lateness. P5-09 (log) needs a read-only "what I set aside" summary over these; if one cannot be assembled, P5-02-style rules work is needed. This is flagged, not assumed (Q4).

## 4. Arrival at Awakening Day, and the four checkpoints

**Arrival.** The final compressed week ends on **Thursday 22 January 2043** (the last weekday before the awakening; `GAME_PLAN.md:§4 Phase 2` gives Friday 23 January 2043 as the end of the year and Awakening Day; `AwakeningInputs.h:AppointmentAt` is 09:00 that day). The tester sleeps and wakes on awakening morning, in their own bed. A short **year receipt** names the three strongest evidence sources, what the projection could not observe, and the sacrificed obligations it can trace (section 3). It judges nothing (`AWAKENING_AND_EXAM.md:The tutorial dungeon`).

**The four checkpoints (M4-16) become outputs of the tester's own run, not authored starts:**

| Checkpoint | Written | Who it serves |
|---|---|---|
| **1. Awakening morning** | At the wake of Friday 23 January 2043, the compression's natural end | Resume point for a tester; **the replay anchor** |
| **2. First countdown evening** | 18:00 on Awakening Day (`Core/P/Countdown.h:CountdownStartInstant`) | Resume after the dungeon, the ten-days opening |
| **3. Exam morning** | 06:00, 2 February 2043 (`AwakeningInputs.h:ExaminationAt`) | Resume the tower |
| **4. Results** | After the examination | Resume the first post-exam week |

Because they come from a real run, **provenance is honest**: "recorded from this life, build X" (`SYSTEMS.md:§6 checkpoint policy`: checkpoints are within a life). The owner and CI still need reproducible starts. So the same 16-year-old can be rebuilt by **replay**: creation save, content hash and command log reproduce the same state hash (`SYSTEMS.md:§2.4`, replay files; `pwreplay` names the first divergence). A replay-sourced start is labelled "replay of a recorded compressed year", never "prepared".

### 4.1 What P5-07 and P5-08 must change

Both assumed a prepared late-year save (`PHASE5_PLAN_2026-11-22.md:P5-07, P5-08`; Q M in §4 of that plan recommended it). Under this decision:
- **P5-07 (store four checkpoints).** Remove "prepared tester starts distinct from player recovery" as the main case. Primary: checkpoints are written automatically as the run reaches each phase, in the player's own slot, as recovery points. Secondary: a **named reference-life set** (below) stored with scripted-policy provenance for CI and M4-19. The old day-loop-only checkpoint payload remains insufficient (`Core/P/Checkpoints.h:WriteDayLoopCheckpoint`). Add a pass: a **compressed-year run produces checkpoint 1 whose state hash equals its replay's**.
- **P5-08 (load in package).** The selector is a recovery/return list, not a start menu. New pass: a fresh process loads checkpoint 1 and the tester continues into the dungeon. A tester's **New Game flow** gains the compression run (a driver screen between Review and Awakening morning: the lead-in, then five decision stops with a resolution screen between them, `SYSTEMS.md:§2.3`).
- New small work (owner to dispatch): a **year driver** that chains week calls, places the five stops, writes the receipt and checkpoint 1. It owns no rules (section 2.1). Suggested order id P5-07a, Claude Opus for the save envelope seam, GPT-6.1 sol for the Unreal screen.

**Reference lives (three, all scripted policy, disclosed):** combat-led, non-combat only, deliberately poor result, matching M4-19 (`ROADMAP.md:§8 M4-19`). Each is a replayable command log, never a hand-built save, so "never invented player history" holds (`PHASE5_PLAN_2026-11-22.md:P5-07`).

## 5. Rank 0 and zero supernatural practice

- **Rank 0 (institutional care).** Every one of the five decisions needs a care-home variant. Existing scenes carry one (`SchoolLifeContent.h:SchoolOriginVariant::CareHome`; the rota has care-worker duty 07:00 to 15:00). The **gap**: the three countdown obligations are family roles (sibling, neighbour, teacher) and the status doc records "no institutional-care obligation variant yet" (`STATUS.md:Checkpoint M4-03`, Not yet done). For the year this is covered by scenes; for the ten days it stays an existing P4 gap and is not reopened here. Care-home lives lose no decision: the rota redesign becomes an arrangement with a care worker rather than a parent, with equal weight and no lesser outcome (`GAME_PLAN.md:§1` dignity; pillar 6).
- **Zero practice.** Nothing before Awakening Day is supernatural. A tester who sets decision 5 to written study, or orders nothing, still reaches the dungeon, which **cannot be refused** (`GAME_PLAN.md:§1`, §12 A). Afterwards every route is optional. The year receipt does not say the player prepared badly; the evidence line simply names what it observed and what it did not.
- **Accessibility.** Every stop can be revisited from the phone before commit; a paused clock and pause-safe saving apply (`GAME_PLAN.md:§5` accessibility pause).

## 6. Verification

| Rung | Pass condition |
|---|---|
| **CTest (portable)** | (1) The year driver, for a fixed seed and policy, produces a deterministic state hash at awakening morning; replay reproduces it. (2) A scripted run that chooses the same decisions as step-by-step play of two compressed weeks matches field by field except excluded quantities (existing `StateDiff`). (3) Each of the five decision stops: refusal, each branch, and duplicate or stale command change nothing the second time. (4) Rank-0 and family lives each reach checkpoint 1; zero-practice policy reaches it. (5) The three reference lives reproduce their hashes. (6) No stop leaves the weekly interrupt count above 1. (7) Awakening seed is untouched across compression (no reseed). (8) The week receipt lists every `decided_by_order` entry. |
| **Editor / PIE** | The lead-in is walkable in both cameras; the five stops render receipts with displaced alternatives; the resolution screen shows progress. |
| **Packaged scenarios** | A fresh process runs creation to checkpoint 1 in one session, quits, relaunches and Continues from checkpoint 1; then the existing dungeon and countdown scenarios run from it. Rank-0 and zero-practice variants each have a packaged record. Both cameras once. |
| **+owner (cannot be automated)** | Owner judgement that the year **feels** like a year, not a menu; the tone of the default-week receipt; whether five decisions are enough or too many; the first blind pass. The M4-20 counts (6 of 8 explain awakening and name a sacrifice) come only from human testers (`PHASE5_PLAN_2026-11-22.md:P5-10`). Scripted runs prove reachability, never discovery. |

Time budget to check at the owner session (proposed): lead-in 15 to 20 minutes, five stops plus resolution 15 to 25, so **about 35 to 45 minutes** before Awakening Day. Tower and countdown follow at their existing lengths (`GAME_PLAN.md:§4`).

## 7. Open questions for the owner

1. **Q1. Five or six decisions, and where.** Accept the five (six with the terminal) and their approximate weeks, or move/replace any. Constraint: at most one stop per week (`WeekMaxInterrupts`).
2. **Q2. The default week.** For weeks the tester orders nothing, is "attend, keep the rota, sleep" the right disclosed default? The alternative is stopping every week, which defeats compression.
3. **Q3. Conduct feeding awakening.** Today conduct is written only inside the dungeon. Do you want the year's decisions to leave an additional, disclosed conduct-like mark, or keep the current split (year shapes stats and relationships; dungeon reads the person)? This proposal assumes the current split.
4. **Q4. The sacrifice record.** Accept a read-only "what I set aside" view over existing receipts and threads, or require a dedicated obligation ledger (new rules, schema, save bump)? Affects M4-17.
5. **Q5. Length.** Is a 35 to 45 minute pre-Awakening lead acceptable for a blind-test session, given M4-20's session length is also open?
6. **Q6. Replay as the reproducible start.** Accept replay-sourced reference lives instead of hand-authored saves, and label them as such to testers?
7. **Q7. Year compression.** The slice chains weeks. Do you accept that, or hold the slice until M5's season/year compression exists?
8. **Q8. Care-home ten days.** Authorise the missing rank-0 countdown obligation variant, or leave rank 0 to the year's scenes only for this slice.

## 8. Related documents

`GAME_PLAN.md:§4, §5, §10, §12`; `ROADMAP.md:§3 R6, §8 M4-06, 16, 17, 20`; `PHASE5_PLAN_2026-11-22.md:P5-07, P5-08`; `AWAKENING_AND_EXAM.md`; `SCHOOL_AND_PROGRESSION.md`; `SYSTEMS.md:§2.4, §2.5, §6`. On acceptance the owner (or Sonnet at review) should amend `PHASE5_PLAN_2026-11-22.md:P5-07/P5-08`, `ROADMAP.md:M4-16`, and add an acceptance row to `GAME_PLAN.md:§10`. This file amends none of them.
