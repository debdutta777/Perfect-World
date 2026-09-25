# Roadmap: M5–M10

**Document status:** execution plan, 2026-09-23. This continues [`ROADMAP.md`](ROADMAP.md) and uses its step format, shorthand and standing rules (§1 there). Gates are copied from `GAME_PLAN.md` §8. These steps are coarser than M0–M4 on purpose. Split each one into bounded steps (`DEVELOPMENT_WORKFLOW.md` §2) when its milestone becomes current, using the lessons from M0–M4. Every step here is **not started**.

---

## M5 — Adult life, plus a skeletal later life and epilogue

**Goal:** the rest of school as a new stage, the first adult routes, economy, housing, ageing and recovery from setbacks, plus a thin later life and epilogue, so that a whole life runs end to end before breadth work begins. **Gate:** *"A non-hunter lives 16 → 30 and holds up; an established worker's week holds up; a whole life runs end to end."* **Playable result:** one life from 16 to death, non-hunter, with the adult years played and the late years skeletal.

| ID | Deliverable | Touches | Pass condition | Rung | Deps |
|---|---|---|---|---|---|
| M5-01 | Season and year compression under the equivalence rule | Core/SeasonResolver, YearResolver | CTest: for three personas, a season resolved in one step gives the same persistent state as its weeks resolved one at a time; every scheduled event fires once | CTest | M3-02 |
| M5-02 | Economy and ownership: money in integer paise, inventory, tools, contracts, debt, household budgets | Core/Economy | CTest: no operation creates or destroys money outside a declared source or sink; debt and insolvency states stay valid; save round trip | CTest | M1-05 |
| M5-03 | Ageing and birthdays from the date of birth; physical, cognitive and social curves (`ALTERNATE_WORLD.md` §6) | Core/Ageing | CTest: birthdays fire on the right date, including for 29 Feb births; curves stay bounded; age never changes except by the calendar | CTest | M0-18, M5-01 |
| M5-04 | The rest of school as a new stage (Phase 6): changed status, board exams separate from the tower, applications, funding, retakes up to age 20 | Core/School; Data; Game | CTest: retake eligibility ends at 20. PIE: the owner judges that the week after results reads as a new stage | PIE +owner | M4-20 |
| M5-05 | Four records and the career framework: methods, permissions, achievements and relationships; careers as problem families with ladders | Core/Career | CTest: changing career keeps methods and relationships; permissions gate tasks, not stats | CTest | M5-02 |
| M5-06 | First careers: three ordinary careers and one path's early ladder, chosen by the owner, each with a first week, a first consequential task, a representative routine and a recovery route | Data/Careers, Data/Layouts | Each career is playable for its first month; the content matrix is updated; no structure is used in a new way without passing the repetition gate | PIE +owner | M5-05, M3-04 |
| M5-07 | Housing: leaving home or care, deposits, rent, transport, commutes | Core/Housing; Data | CTest: rent and deposits go through the economy. PIE: move into a rented room and commute | PIE | M5-02 |
| M5-08 | Life events and recovery: unemployment, dismissal, insolvency, retraining, career change | Core/LifeEvents | CTest: every event has at least one recovery route reachable within a year. PIE: one of each is played | PIE | M5-05 |
| M5-09 | Relationships and households: friendship, shared finances, care, living together, estrangement, bereavement. Romance as §12 D decides | Core/Households | CTest: shared finances balance; bereavement never closes a thread silently | CTest | M3-06, §12 D |
| M5-10 | Skeletal later life: established and later-life phases with one project, mentoring, a closure interval, and death in old age that is signalled in advance | Core; Data | CTest: death is signalled at least one season ahead; the closure interval always happens | PIE | M5-03 |
| M5-11 | Skeletal epilogue, drawn from the event log: people, places, work, ordinary memories, unresolved matters, optional ranking history | Game; Core query | Every epilogue line traces back to a logged event | PIE | M5-10, M2-05, §12 C |
| M5-12 | Whole-life soak: four headless personas, one of them a stable ordinary life, from 16 to death | Tests/LifeSoakTests.cpp | CTest (long-running label): no invalid state; replay-stable; runtime and save size within budget | CTest | M5-01 to M5-11 |
| M5-13 | Established-worker test: a 34-year-old's week covering stable work, care, a career change and recovery from failure | STATUS.md | The owner and two testers judge that it holds up; findings labelled | PIE +owner | M5-06, M5-08 |
| M5-14 | M5 gate run: a non-hunter lives 16 → 30, and a whole life runs end to end | STATUS.md | The owner's playthrough, M5-12 and M5-13 all recorded | Cooked +owner | M5-01 to M5-13 |

---

## M6 — The living nation

**Goal:** people exist without the player at every fidelity. Stable identity, the event log and minimal off-screen progress are already built (M2-05, M2-06, M3-12), so M6 stress-tests them at the scale of decades. **Gate:** *"Demotion, re-encounter, cohort replacement and causal attribution hold across decades."* **Playable result:** a decade of Indian life that moved without the player and can explain itself.

| ID | Deliverable | Touches | Pass condition | Rung | Deps |
|---|---|---|---|---|---|
| M6-01 | Simulation fidelity tiers and update cadence (weekly, monthly, seasonal, annual); changing a person's fidelity never changes their identity | Core/Fidelity | CTest: cost per simulated year within budget; identity hash unchanged by tier changes | CTest | M5-12 |
| M6-02 | Crystallisation from cohort distributions, with history backfilled from the event log | Core/Crystallisation | CTest: generated history never contradicts the log; distribution statistics hold over 10,000 draws | CTest | M6-01, M2-05 |
| M6-03 | Demotion and re-encounter: after two years unseen a person becomes a summary; meeting them again restores the same person | Core/Fidelity | CTest: re-encounter restores identical identity and history; the summary is deterministic | CTest | M6-02 |
| M6-04 | Cohort replacement: births, school cohorts, migration, deaths, population aggregates | Core/Population | CTest: over 30 years each nation's aggregates stay inside their authored bands | CTest | M6-01 |
| M6-05 | Causal attribution: every major outcome records its causes, the parties involved and the player's contribution, and can answer "why" | Core/Attribution | CTest: every hazard consequence lists its causes; contributions add up consistently | CTest | M2-05 |
| M6-06 | National rankings and the competition calendar, school level to national (best four results in 24 months) | Core/Rankings; Data | CTest: fifty small events cannot beat four good ones; rankings publish on schedule | CTest | M6-04 |
| M6-07 | The twelve world mechanisms (`ALTERNATE_WORLD.md` §4) as data with effects | Data; Core | Each mechanism has a test showing its effect on an ordinary life | CTest | M6-05 |
| M6-08 | A changing world: institutions evolve, businesses turn over, technology spreads | Core; Data | CTest: over 30 years turnover stays within bands, and no named place or person disappears silently | CTest | M6-04 |
| M6-09 | Scaling stress test: a 40-year headless run with the full cast | Tests | Time per season, memory and save size within budget; replay-stable | CTest | M6-01 to M6-08 |
| M6-10 | M6 gate run: within a decade the owner meets a demoted person again, sees a replacement cohort, and asks "why" of three outcomes | STATUS.md | All four hold | Cooked +owner | M6-01 to M6-09 |

---

## M7 — The world

**Goal:** give the thin thirty-nation model from M3-10 its behaviour: Country Level, the Convergence, the tensions, first travel. The world must reach ordinary lives as well as champions. **Gate:** *"The player's standing moves India's rating understandably — and a player with negligible influence still feels the world."* **Playable result:** a life in which the world visibly moves, whether or not the player moves it.

| ID | Deliverable | Touches | Pass condition | Rung | Deps |
|---|---|---|---|---|---|
| M7-01 | Nation behaviour: seasonal updates for thirty nations; notables' careers advance | Core/Nations | CTest: over 20 years every nation stays valid and every notable persists | CTest | M3-10, M6-01 |
| M7-02 | Country Level: each path contributes exactly 10%; inputs change continuously, the value is published quarterly and reviewed annually | Core/CountryLevel | CTest: the composite formula holds; the published value changes only at quarter boundaries | CTest | M7-01 |
| M7-03 | Country Level in daily life: prices, travel, recognition, pressure | Core; Game | CTest: effects stay bounded. PIE: a band change shows up in rent and in the news | PIE | M7-02, M5-02 |
| M7-04 | World Convergence: ten disciplines, sixty entries each, every year | Core/Convergence; Data | CTest: entries are allocated by rating. PIE: it is broadcast on the phone | PIE | M7-02 |
| M7-05 | Five global tensions with authored states and thresholds | Core/Tensions; Data | CTest: transitions fire on dates or thresholds, never from player action alone | CTest | M7-01 |
| M7-06 | Making the player's contribution readable: their standing moves the rating, with an explanation the player can follow | Game; Core/Attribution | A tester can explain why India's rating moved | PIE +owner | M7-02, M6-05 |
| M7-07 | First travel: one foreign location as playable space | Data/Layouts; Game | Travel costs time and money; the location passes a cultural review | PIE +owner | M7-03, §12 B |
| M7-08 | Negligible-influence test: an ordinary life still feels the world through prices, news, relatives abroad and tensions | STATUS.md | The owner and one tester, both on ordinary lives, can each name three ways the world reached them | PIE +owner | M7-03 to M7-05 |
| M7-09 | M7 gate run with a champion save and an ordinary save | STATUS.md | Both halves of the gate hold | Cooked +owner | M7-01 to M7-08 |

---

## M8 — Breadth

**Goal:** every path, the remaining places and phases, with content paced to what the owner can review, and audited for repetition. **Gate:** *"Complete lives with fresh testers across the scenario matrix: stable ordinary life, homemaking and care, late career switch, hybrid career, summit pursuit, recovery after serious failure."* **Playable result:** a complete 60–120 hour life on any path.

| ID | Deliverable | Touches | Pass condition | Rung | Deps |
|---|---|---|---|---|---|
| M8-01 | All ten world-summit paths and about 13 ordinary careers, each with a first week, a first consequential task, a routine and a recovery route | Data/Careers | Validator passes; the content matrix is complete for every path and phase | CTest +owner | M5-06 |
| M8-02 | Problem families up to about 300, added no faster than the owner can accept them | Data/Problems | Validator passes; the owner accepts every family; any new use of a structure has passed the repetition gate | CTest +owner | M8-01 |
| M8-03 | One or two further cities, and the rest of the block interior suite: shop, ward, office | Data/Layouts | Audit 0; within budgets | PIE | §12 F |
| M8-04 | Full established and later life (Phases 9–10): expert practice, second careers, mentoring beyond one's own children, new projects late in life | Data; Core | Each phase meets the phase completeness rule (`GAME_PLAN.md` §4) | PIE +owner | M5-10 |
| M8-05 | Full plural epilogue, and legacy play as §12 C decides | Game; Core | Every line traces to the log; legacy works as decided | PIE | M5-11, §12 C |
| M8-06 | Repeated-content audit on three axes: decision structure, social role, consequence | Tools; Data/Matrix | The report lists every repeat; the owner keeps or cuts each one | CTest +owner | M8-02 |
| M8-07 | Playtime reconciliation: measured complete-life runs against the 60–120 hour envelope | Data/Matrix; STATUS.md | The budget is updated from measured runs, and every gap is named | +owner | M8-04 |
| M8-08 | Decisions on §12 D and E applied throughout | Data; validator | The validator enforces the chosen rating | CTest | §12 D, §12 E |
| M8-09 | M8 gate: the scenario matrix, played by fresh testers | STATUS.md | Each of the six scenarios is completed by a tester who has not played before, and judged to hold up | Cooked +owner | M8-01 to M8-08 |

---

## M9 — Art integration

**Goal:** replace every block placeholder with real art through both import routes, prove that nothing else changed, then accept the final presentation. **Gate:** *"Three separate tests: asset coverage and provenance; no unintended change to rules, saves, collision contracts or outcomes; final-presentation acceptance for readability, contact, comfort, performance, accessibility and tone. Includes a scheduled presentation-adjustment period."* **Playable result:** the M8 game in final art, with the same outcomes.

| ID | Deliverable | Touches | Pass condition | Rung | Deps |
|---|---|---|---|---|---|
| M9-01 | Provenance ledger for every asset ID (source, licence, author or AI tool, date), and a coverage report | Tools; `ART_PROVENANCE.md` | The report lists every ID as block or real, with its provenance | Editor | M1-07 |
| M9-02 | Drop-folder import route | Per `ASSET_PIPELINE.md` | A dropped FBX and a dropped GLB each import, bind to their ID and pass M9-04 | Editor | M9-01 |
| M9-03 | Editor import route | Per `ASSET_PIPELINE.md` | The same asset imported through the editor gives the same result | Editor | M9-01 |
| M9-04 | Contract checker: a real asset must fit its contract (visual envelope, anchors, clearance, performance allocation) | Tools | Rejects a deliberately oversized asset; accepts a conforming one | Editor | M9-02 |
| M9-05 | Final character meshes on the frozen skeleton, keeping every animation | Content/Characters | All animations play; the appearance record drives the mesh | PIE +owner | M9-04 |
| M9-06 | Bulk import in waves by asset family, with art supplied or approved by the owner | Content | Each wave passes M9-04 and audit 0 | Editor +owner | M9-04 |
| M9-07 | **Test (a):** asset coverage and provenance at 100% | STATUS.md | The coverage report reads 100%, and every asset has provenance | Editor | M9-06 |
| M9-08 | **Test (b):** no unintended change. Recorded command logs replay to identical hashes; every migration fixture loads; collision contracts and outcomes are unchanged | Tests; automation | CTest and automation pass against the pre-art baselines | Cooked | M9-06 |
| M9-09 | **Test (c):** final-presentation acceptance for readability, contact, comfort, performance, accessibility and tone, with block-era findings retested | STATUS.md | Testers and the owner sign off each area | Cooked +owner | M9-07, M9-08 |
| M9-10 | Scheduled presentation-adjustment period (the owner sets its length), then tests (b) and (c) re-run | STATUS.md | Both pass after the adjustments | Cooked +owner | M9-09 |

---

## M10 — Ship

**Goal:** final certification. Budgets (from M1-11), accessibility (from M1-13), string tables (from M1-13) and cooked builds (from M0-15) were started early, so M10 certifies and packages rather than retrofitting. **Gate:** *"Final certification. Performance, accessibility, localisation support and cooked builds began at their earlier milestones."* **Playable result:** a release candidate.

| ID | Deliverable | Touches | Pass condition | Rung | Deps |
|---|---|---|---|---|---|
| M10-01 | Performance certification on the named minimum spec | STATUS.md | Budgets hold across a full-life route | Cooked | M9-10, R4 |
| M10-02 | Accessibility certification: a whole life completed with a controller alone; remapping, subtitles, text scale, cues that do not rely on colour, the accessibility pause | STATUS.md | Testers complete it with no blocking issue | Cooked +owner | M9-10 |
| M10-03 | Localisation as §12 G decides | String tables | Every string localised in the chosen languages; no truncation at 200% text scale | Cooked | §12 G |
| M10-04 | Android, per the 2026-09-20 decision (a feasibility spike may come earlier: `ROADMAP.md` R7) | Build scripts | Launch, touch, suspend and resume, and budgets, all on a named device | Cooked | R7 |
| M10-05 | Save compatibility: every migration fixture since v1 loads in the release build | Tests | CTest and a cooked load both pass | Cooked | M9-08 |
| M10-06 | Store build pipeline, content rating (§12 E), legal, credits, business model (§12 H) | Scripts; Docs | The store build uploads; a rating is obtained | +owner | §12 E, §12 H |
| M10-07 | Release candidate: full-life regression suite and owner sign-off | STATUS.md | Every suite passes, and the owner signs off | Cooked +owner | M10-01 to M10-06 |
