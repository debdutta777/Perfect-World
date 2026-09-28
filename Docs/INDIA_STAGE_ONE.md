# India, stage one: cities, Gates, education and society

**Document status:** design, 2026-09-28, on the owner's instruction to "build India and its cities, dungeon, schools of various levels and universities, resources and social classes". It sits under `GAME_PLAN.md`, `WORLD_SCALE.md` (how cities and the Deep Realm are built), `INDIA_WORLD_BIBLE.md` (Kolkata and Nabapara, the cultural authority) and `GLOBAL_WORLD.md` (nations and rankings). **The lists live as data in `Data/Content/<type>/<id>.json`, checked by the content validator (M1-06); this document gives the rules those files follow.** Every institution, school, company and person is **fictional**; cities, rivers and neighbourhoods are real. All of it needs the cultural review `INDIA_WORLD_BIBLE.md` requires before any of it appears as fact in the game. Not yet reviewed by GPT-6.

---

## 1. Stage one scope

| City | Role in stage one | Hero district (authored) | Generated districts |
|---|---|---|---|
| **Kolkata** | The opening. Nabapara, school, home, the tutorial dungeon | Nabapara (fictional para) | Old north core, riverfront, market belt, apartment belt, rail yard |
| **Delhi** | National capital: NGA headquarters, the national tower examination hall, the national Gate academy | Fictional NGA civic quarter | Government quarter, old core, apartment belt, campus |
| **Mumbai** | Enterprise and port: Gate-materials trade, finance, the coastal Gate belt | Fictional harbour works | Port, dense core, apartment belt, informal settlement |
| **Bengaluru** | Research and engineering: Tattva instrumentation, the largest research university | Fictional research campus | Campus, apartment belt, industrial, market |
| **Chennai** | Medicine and manufacturing: Gate-syndrome clinics, heavy industry, the eastern coastal Gates | Fictional medical district | Industrial, port, dense core, suburb |

Travel between them is by rail and air, as scenes with declared time costs (`WORLD_SCALE.md` §2).

## 2. Gates and India's dungeon

- **City Gates.** Each city has registered Gate sites (`Data/Content/city/`), each with a rank from F to S. Most are F–D and contained by the National Gate Authority; a few higher-rank sites are sealed zones. Onsets are scheduled world events; consequences depend on preparation (`GAME_PLAN.md` §2).
- **India's home region of the Deep Realm.** Under the shared-realm decision (`WORLD_SCALE.md` §7, W3), India's Gates open into one home region of floor 1. Each city's major Gate is an **anchor**: a fixed, surveyed entry point in the realm. Anchors are close to each other in the realm even though the cities are far apart on Earth, which makes the realm India's fastest shared frontier and a reason for inter-city rivalry.
- **Every school's tutorial dungeon** is a supervised F-rank instance, the same for every student (`AWAKENING_AND_EXAM.md`). It is not part of the Deep Realm.
- **The Tower Examination** is virtual and run centrally from Delhi, with a certified hall in every stage-one city.

## 3. The education ladder

India's structure is followed in outline, as an alternate-world version. Real board names, fees and rules are **not** asserted; the school bible's caution applies (`INDIA_WORLD_BIBLE.md`).

| Stage | Ages (common) | Notes |
|---|---|---|
| Pre-primary | 3–6 | Background only in stage one |
| Primary (Classes I–V) | 6–11 | Background; appears in family and younger-sibling stories |
| Upper primary (VI–VIII) | 11–14 | Background |
| Secondary (IX–X) | 14–16 | Ends with the Class X board examination, already behind the player at the start |
| **Higher secondary (XI–XII)** | **16–18** | **Where the game opens.** Science, Commerce or Humanities streams. The awakening and the tower examination fall in Class XI; Class XII boards follow in Phase 6 |
| Vocational and polytechnic | 16+ | Trades, diplomas, apprenticeships; a full route to every summit (`PATHS_AND_PROGRESSION.md`) |
| Open schooling | any | Flexible study for those who work, care or left school |
| Undergraduate | 18–22 | Colleges and universities |
| Postgraduate and doctoral | 21+ | Research and professional degrees |
| **Gate academies** | 17+ (after awakening) | Accredited training in Gate operations and Tattva practice; they certify grades up to C (`AWAKENED_GRADES.md`) and prepare licence candidates |

### School types

Stage one uses five kinds of school, never ranked by the worth of their students:

| Type | What it means in play |
|---|---|
| Government school | Free or nearly free; large classes; committed and overstretched staff; strong community ties |
| Government-aided school | Public funding with its own management. **Nabapara Vidyalaya is this kind** |
| Private budget school | Low fees, uneven facilities, often closer to home |
| Private premium school | High fees, facilities and networks; pressure of a different kind |
| Residential academy | Boarding; some are sports or Gate-preparation academies with scholarships |

### University tiers

| Tier | What it offers |
|---|---|
| **National institutes** | Highly selective, nationally funded, research-heavy; tower-examination rank and entrance tests both count |
| **Central universities** | Broad national universities with strong departments |
| **State universities** | The largest intake; quality varies by department, not by prestige alone |
| **Private universities** | From excellent to poor; fees and scholarships matter |
| **Gate research universities** | New since the Gates: Tattva science, Gate medicine, containment engineering |

University rankings follow `GLOBAL_WORLD.md` §4: **student development counts 20%**, so an institution that teaches weak intakes well can outrank a coasting elite one. That is visible in the fiction.

## 4. Social strata and resources

### Household economic strata

Class in this game is **economic and spatial**, shown through space, time, risk, privacy, commute and buffers (`INDIA_WORLD_BIBLE.md`, "Money and class"). The five strata map to the creator's **Household Resources rank** (`CHARACTER_CREATION.md`):

| Rank | Stratum | Shows up as |
|---|---|---|
| 0 | **Institutional care** | A children's home: shared rooms, institutional routines, caseworkers, a leaving-care cliff at 18 |
| 1 | **Precarious** | Daily or informal income; one room or a shared flat; long commutes; no buffer for a repair or a hospital bill |
| 2 | **Working** | Steady but modest wages; a small flat; careful monthly budgets; savings for exams |
| 3 | **Middle** | Salaried household; a flat of its own; tuition affordable; a buffer for one emergency |
| 4 | **Comfortable** | Property, a car, domestic help, coaching and travel; risk of complacency and expectation |

Rules: a stratum is a starting position, not a verdict. It never sets intelligence, virtue, English fluency or family warmth. Mobility happens both ways through work, education, illness, luck and Gate events. **Caste and religion are not game mechanics.** They exist in people's lives, and any writing that touches them goes through the cultural review; no system reads them and no stat depends on them.

### Resources a life runs on

| Resource | Scope | Notes |
|---|---|---|
| **Time** | Personal | The central currency (`GAME_PLAN.md` pillar 2) |
| **Energy, health, stress** | Personal | Condition (`SYSTEMS.md`); ordinary neglect never kills |
| **Pocket money and earnings** | Personal | In ₹, from a consistent price table |
| **Household budget and reserve** | Household | Absorbs repairs and fees; not lootable (`INDIA_WORLD_BIBLE.md`) |
| **Credentials** | Personal | Board results, tower rank, degrees, grades, licences |
| **Relationships and reputation** | Social | Four records (`GAME_PLAN.md` §2) |
| **Gate materials** | Economy | Cores, residues and crystals graded F to S by the Gate they came from; licensed collection only; traded, researched and manufactured into instruments, medicine and containment |
| **Public goods** | City | Transit, hospitals, schools, Gate defences; their quality depends on city and national spending, which Country Level reflects |

## 5. Awakening in India

Every Class XI student in the country awakens on the same day through their school's tutorial dungeon (`AWAKENING_AND_EXAM.md`). Grades, levels and promotion follow `AWAKENED_GRADES.md`. The NGA holds the national grade register; Gate academies and the NGA assess promotions; licences are issued separately.

## 6. What stage one builds, in order

The steps are in `ROADMAP.md` (M1-32 onward, with M1-25 to M1-31 for the world-scale machinery):

1. India data: the nation, the five cities with districts and Gate sites, institutions, education stages, strata (`Data/Content/`, M1-32).
2. The awakened-grade rules in the portable core (M1-33).
3. A validator over `Data/` (M1-06), then Nabapara in 3D (M1-21, M1-22, M1-30).
