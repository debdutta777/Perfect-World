# The global world: thirty nations

**Document status:** core design, 2026-09-20. Authoritative on nations, country level, rankings, the competition calendar and global tension. Answers to `VISION.md`. Nothing here is implemented.

*National fiction, pending owner decision, 2026-09-23 (`GAME_PLAN.md` §12 question B).* The recommended reading, which the rest of this document already follows: **real countries exist as alternate-world versions of themselves; every institution, organisation, company, person, power bloc and incident is fictional; no real-world conflict is depicted directly.** An earlier line here said "all nations are fictional" while the examples used India, Korea, Brazil and Singapore. Both can't be true, and the owner's decision settles which one is.

## 1. The governing principle

**A world feels global through rankings, news and consequence — not through walkable geography.**

The player will never visit twenty-nine of the thirty nations, and does not need to. What makes a world planetary is knowing where you stand in it, hearing what happened elsewhere, and feeling it arrive in your street four months later.

This is also what makes the scope tractable: **global reach is mostly data.** Data is cheap, deep, testable, and exactly what AI agents build well.

### The three-way split

Every element of the world sits in exactly one category. This line is load-bearing.

| Category | What it is | Cost | Examples |
|---|---|---|---|
| **Playable space** | Places the player physically occupies | Very expensive — art, level design, navigation, audio | Nabapara, the school, home interiors, the tower, one or two later cities |
| **Simulated data** | State that updates, that the player reads and is affected by | Cheap — records and maths | All thirty nations' ratings, rankings, economies, NPC cohorts, competition results, migration, prices |
| **Authored event** | Fixed dramatic beats on the world timeline | Moderate — writing, not geometry | The disasters, treaty crises, the Convergence, named incidents |

**The rule: a nation becomes playable space only when the story requires the player's body to be there.** Everything else about it is data and authored event. Brazil can matter enormously to a player who never leaves Kolkata.

### What Korea is, concretely, to a player who never goes there

- A **rating** — currently 2nd, having overtaken India two years ago, which was in the news for a week
- **Named figures** — a world No. 1 expedition specialist whose Convergence final the player watched; a researcher whose containment method your workplace now uses
- **A price effect** — their equipment standard is cheaper, so your brother's technician job is threatened
- **A migration pull** — your sister is learning the language
- **A rivalry** — the India–Korea junior semi-final is a national event, and your school ran a sweepstake
- **An authored crisis** — in year four their containment failure kills 400 people, global ratings shift, and the compact renegotiation reaches your evening news

That is a nation the player *feels*, built entirely from records, news templates and four authored scenes. **No geometry.**

## 2. Country Level

Every nation carries a public composite rating: **Country Level, 1–100.** It is the number the entire world argues about.

### Inputs

Ten paths, each exactly 10% — the same structure as the individual ranking in `PATHS_AND_PROGRESSION.md`, and for the same reason: **one dominant hunter cannot cover failure in nine other fields.**

Within each path: **70% senior/open results, 30% junior and university results.** A nation that neglects its schools decays with a decade's delay, which is exactly the point.

Additional modifiers applied to the composite, not to any single path:

| Modifier | Effect | Why |
|---|---|---|
| **Containment rate** | Gates neutralised before civilian casualties, rolling 24 months | The single most-reported statistic in this world |
| **Casualty rate per incident** | Negative | Competence, not bravado |
| **Research adoption abroad** | Positive | Contribution to the whole world counts |
| **Institutional stability** | Positive | Ratings reward states that function |
| **Compact compliance** | Positive | International obligations met |

### What Country Level actually does to your life

This must be concrete or it is a number on a screen.

| Band | Name | What it means where you live |
|---|---|---|
| **85–100** | Tier One | Full travel rights. Equipment export licences. Your qualifications are recognised everywhere. Foreign students arrive. Insurance is cheap |
| **70–84** | Established | Most travel rights. Reciprocal recognition with peers. Stable prices |
| **50–69** | Developing | Visa friction. Qualifications need bridging exams abroad. Insurance premiums noticeably higher. **India starts here** |
| **30–49** | Strained | Restricted travel. Equipment import dependency. Emigration pressure. Foreign aid missions |
| **1–29** | Crisis | Containment assistance from abroad, with the political humiliation that carries |

**Concretely, as India's level moves:**

- **Rising:** the certification you are studying for becomes recognised in four more countries. Your premium falls ₹300 a month. Your sister's emigration argument weakens. Your school gets a new lab.
- **Falling:** your employer loses an export contract and freezes hiring. The queue for your mother's treatment lengthens. Emigration becomes the family's main conversation. A politician you dislike gains support.

**The player can move it.** Not alone, and not quickly. A world-ranked researcher moves it by ~2. A national champion by ~1. A cohort of excellent teachers moves it by ~3 across fifteen years — slower, larger, and almost invisible while it happens. That asymmetry is a design statement.

## 3. The competition calendar

One year, repeating, with the World Convergence as its spine.

| Period | Level | Events |
|---|---|---|
| **Jan–Mar** | School & district | School championships, district qualifiers, the **National Unified Examination** (tower climb) for the year's sixteen-year-olds |
| **Apr–Jun** | University & open | University championships in all ten disciplines; open national qualifiers; junior national championships |
| **Jul–Sep** | National | **National Championships** — junior, university and open divisions in each of the ten disciplines. National team selection |
| **Oct** | Continental | Regional meetings. Asia, Europe, Africa, Americas, Oceania circuits |
| **Nov–Dec** | World | **The World Convergence.** Ten world finals, sixty entries each: one guaranteed representative per nation plus thirty merit qualifiers, capped at three per nation |

**Every discipline has the same shape**, so a researcher's year and a hunter's year have identical rhythm and identical prestige. This is the calendar-level expression of "no path is the lesser path."

### A single player's year

A 19-year-old university researcher in Kolkata:

- **February** — helps at the school examination as an alumnus invigilator. Sees the projection terminal from the other side.
- **April** — university championship. Places 4th nationally in the Unknown Law Challenge. Qualifies for open.
- **July** — national championship. 2nd. Selected as India's guaranteed representative.
- **October** — Asia circuit in Singapore. **First time leaving the country.** This is a playable scene because the story requires their body there.
- **November** — World Convergence. 31st of 60. The commentary mentions her name once.
- **December** — a laboratory in Seoul emails about her method.

That year contains travel, ranking, international recognition and a thread that will run for a decade — and required exactly one new playable location.

## 4. Ranking mathematics

Concrete, because vagueness here produces an ungameable or trivially-gamed system.

### Individuals

**Best four sanctioned results over a rolling 24 months.** Each result scores `event_tier_weight × placement_score × field_strength`.

- `event_tier_weight`: school 1, district 2, university 5, national 12, continental 25, world final 60
- `placement_score`: `100 × (1 − (rank−1)/entries)^1.5` — top-heavy but not winner-take-all
- `field_strength`: mean rating of the top 8 entrants ÷ global mean

**Best-four-of-24-months** is the anti-grind mechanism: entering fifty small events cannot outscore four good ones, because only four count and small events carry low tier weight. Rating decays only by results ageing out, never by a punitive clock.

**All-time achievement records never decay.** Current ranking and historical standing are separate displays, always.

### Universities

Ten departmental rankings plus a composite: **50% competitive performance, 30% independently verified work, 20% student development.** Each normalised within discipline. **Counted results are capped per institution** so enrolment size cannot buy rank.

Student development is measured as *improvement relative to intake projection* — which makes an institution that takes weak students and teaches them well rank above one that takes strong students and coasts. That is a deliberate statement and it should be visible in the fiction.

### Nations

Country Level as in section 2. Published quarterly. **Arguing about the methodology is part of the world's culture** — commentators, politicians and academics dispute the weightings, and the game lets them, because a contested number is more alive than an authoritative one.

## 5. Thirty nations without simulating thirty nations

Per nation, per season, the simulation updates:

- **Tier 0 named figures** — 3–8 per nation. World-ranked individuals in a few disciplines. Full records.
- **Tier 1 significant** — ~20 per nation. Rising contenders, officials, institution heads.
- **Tier 2 cohorts** — statistical distributions per discipline, per age band. An individual crystallises at their **first consequential reference**, which in this layer is usually being named in a published result table or a news story, not meeting the player (see `ALTERNATE_WORLD.md` §7). A name that appears in a Brazilian championship result is a permanent person from that moment on.
- **Tier 3 aggregates** — population, economy, casualties, sentiment, migration flows.

**Competition results are resolved statistically, not simulated.** A national championship in Brazil samples its cohort distribution, applies field strength, and produces a result table with plausible names. Only events the player attends are played.

**Cost:** thirty nations × ~28 tracked individuals = ~840 records on a seasonal update, plus distribution maths. This runs in milliseconds and saves in kilobytes.

## 6. Five global tensions

Fictionalised power blocs. Each is a slow thread the player experiences from Kolkata, and each can be moved.

**1. The Containment Compact.** The treaty governing cross-border Gate response. Tier One nations want response rights inside weaker nations' borders; those nations call it occupation by another name. *Reaches Kolkata as:* foreign response teams at an incident, and a furious national argument about sovereignty.

**2. Equipment monopoly.** Three nations produce most certified equipment. Import dependency is the central economic fact for everyone else. *Reaches Kolkata as:* your technician brother's job, the price of your school's lab kit, a licensing scandal.

**3. The awakening-age dispute.** Some nations quietly awaken students earlier, gaining years of development. It is prohibited and widely suspected. *Reaches Kolkata as:* accusations after a Convergence result, a doping-style testing regime, and the terrible question of whether India should do it too.

**4. Migration of the capable.** Tier One nations recruit aggressively from Tier Three. The nations that train talent lose it. *Reaches Kolkata as:* your sister's emigration, your best teacher leaving, a bonded-service policy debate.

**5. Frontier Gates.** A handful of Gates nobody has cleared, in contested or international territory. Whoever clears one first gains enormous standing. *Reaches Kolkata as:* the decade's biggest news story, a national obsession, and the moment the game's timeline pivots.

### Escalation

Tensions escalate on the world timeline regardless of the player. Each has authored states, and transitions fire on scheduled dates or when world conditions cross thresholds — a containment failure above a casualty count, a Country Level crossing a band, a frontier Gate cleared.

**The player moves them only through standing.** A world-ranked diplomat negotiates the compact directly. A journalist exposes the awakening-age violation. A researcher's method breaks the equipment monopoly. **An accounts clerk signs a petition, and eleven months later it is one of four thousand signatures cited in a ruling.** All three are real participation at their own scale — and the third is the one most players will actually live.

## 7. Delivery layers

Mapped to `ALTERNATE_WORLD.md` §9. Each layer is globally-flavoured from the start.

| Layer | Global content | How global it feels |
|---|---|---|
| **1** | News feed, Country Level displayed, other nations named in broadcasts | You know India is 58th-percentile and that a Korean team won. You have never left Nabapara |
| **2** | The examination is explicitly national and simultaneous worldwide | Your climb is one of millions happening at the same hour |
| **3** | Rankings visible, Convergence watched annually, price and migration effects live | The world visibly moves past you, and you feel it in your rent |
| **4** | Full thirty-nation simulation, competition calendar, the five tensions escalating | A living planet that argues about itself |
| **5** | First international travel; one or two foreign locations as playable space | You finally stand somewhere else, and it has been real to you for years |

**Layer 1 already feels global** — through a news feed and one number. That is the proof the principle works.
