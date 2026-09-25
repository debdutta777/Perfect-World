# The global world: fifty nations

**Document status:** core design, 2026-09-20. Authoritative on nations, country level, rankings, the competition calendar and global tension. Answers to `VISION.md`. Nothing here is implemented.

*National fiction, owner decision 2026-09-25 (`GAME_PLAN.md` §12 question B).* **Fifty real countries exist as alternate-world versions of themselves, with real cities and places. People are invented, or inspired by real roles and archetypes, never real. Institutions, organisations, companies, power blocs and incidents are fictional, and no real-world conflict is depicted directly.** The count was thirty until that date. The roster and the full rules are in §8.

## 1. The governing principle

**A world feels global through rankings, news and consequence — not through walkable geography.**

The player will never visit forty-nine of the fifty nations, and does not need to. What makes a world planetary is knowing where you stand in it, hearing what happened elsewhere, and feeling it arrive in your street four months later.

This is also what makes the scope tractable: **global reach is mostly data.** Data is cheap, deep, testable, and exactly what AI agents build well.

### The three-way split

Every element of the world sits in exactly one category. This line is load-bearing.

| Category | What it is | Cost | Examples |
|---|---|---|---|
| **Playable space** | Places the player physically occupies | Very expensive — art, level design, navigation, audio | Nabapara, the school, home interiors, the tower, one or two later cities |
| **Simulated data** | State that updates, that the player reads and is affected by | Cheap — records and maths | All fifty nations' ratings, rankings, economies, NPC cohorts, competition results, migration, prices |
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
| **Nov–Dec** | World | **The World Convergence.** Ten world finals, eighty entries each: one guaranteed representative per nation plus thirty merit qualifiers, capped at three per nation |

**Every discipline has the same shape**, so a researcher's year and a hunter's year have identical rhythm and identical prestige. This is the calendar-level expression of "no path is the lesser path."

### A single player's year

A 19-year-old university researcher in Kolkata:

- **February** — helps at the school examination as an alumnus invigilator. Sees the projection terminal from the other side.
- **April** — university championship. Places 4th nationally in the Unknown Law Challenge. Qualifies for open.
- **July** — national championship. 2nd. Selected as India's guaranteed representative.
- **October** — Asia circuit in Singapore. **First time leaving the country.** This is a playable scene because the story requires their body there.
- **November** — World Convergence. 31st of 80. The commentary mentions her name once.
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

## 5. Fifty nations without simulating fifty nations

Per nation, per season, the simulation updates:

- **Tier 0 named figures** — 3–8 per nation. World-ranked individuals in a few disciplines. Full records.
- **Tier 1 significant** — ~20 per nation. Rising contenders, officials, institution heads.
- **Tier 2 cohorts** — statistical distributions per discipline, per age band. An individual crystallises at their **first consequential reference**, which in this layer is usually being named in a published result table or a news story, not meeting the player (see `ALTERNATE_WORLD.md` §7). A name that appears in a Brazilian championship result is a permanent person from that moment on.
- **Tier 3 aggregates** — population, economy, casualties, sentiment, migration flows.

**Competition results are resolved statistically, not simulated.** A national championship in Brazil samples its cohort distribution, applies field strength, and produces a result table with plausible names. Only events the player attends are played.

**Cost:** fifty nations × ~28 tracked individuals = ~1,400 records on a seasonal update, plus distribution maths. This runs in milliseconds and saves in kilobytes.

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
| **4** | Full fifty-nation simulation, competition calendar, the five tensions escalating | A living planet that argues about itself |
| **5** | First international travel; one or two foreign locations as playable space | You finally stand somewhere else, and it has been real to you for years |

**Layer 1 already feels global** — through a news feed and one number. That is the proof the principle works.

## 8. The fifty nations

**Owner decision, 2026-09-25:** fifty real countries, up from thirty. The rules below are binding. The roster is **proposed**: the owner confirms it before M3-10 turns it into data. Swapping a nation costs nothing until then; after that, its ID is permanent.

### What is real and what is invented

| Real | Invented |
|---|---|
| Countries, under their common English short names | Every person who appears as a character |
| Geography, cities, regions, landmarks, climate | Governments in office, leaders, parties and laws of the Gate era |
| Languages, festivals, food, dress, cultural calendars | Institutions: agencies, universities, schools, companies, teams, leagues, media |
| History, broadly, up to the Gates | Power blocs, treaties and incidents, including every Gate event |

1. **People are invented, or inspired, never real.** A character may draw on a real role, era or archetype: a veteran cricket captain, a reforming chief minister, a famous physicist. They never carry a real person's name or biography and are never identifiable as a particular person. Real historical and cultural figures may be *referred to* as part of history and culture, such as a song, a statue or a festival, but never appear as characters. No real person is assigned a fictional scandal, crime or Gate casualty. This extends `INDIA_WORLD_BIBLE.md`'s rule to every nation.
2. **Institutions follow the National Gate Authority pattern:** invented, plausible and locally legible. A real university, company or government body never appears under its real name.
3. **No real-world conflict is depicted directly.** Wars, territorial disputes and atrocities are not the game's material. Tension between nations runs through the five fictional tensions (§6) and fictional power blocs.
4. **Maps show nations as named regions and cities, not drawn borders**, so the game never rules on a disputed boundary. Any drawn world map is checked against the law of every market it ships in, India's included.
5. **No nation is a caricature.** Nationality grants no innate stat bonus (`PATHS_AND_PROGRESSION.md`), and every nation's cast includes ordinary people living ordinary lives. The dignity rule in `VISION.md` applies to countries as well as to individuals.
6. **Selection:** UN member states only, so the roster never rules on recognition disputes. Nations are chosen for population, economic and cultural weight, and so that each continental circuit (§3) has a real field.

### The roster

Stable IDs are `nation_` plus the lowercase ISO 3166-1 alpha-3 code. Display names can change without a save migration; IDs cannot.

| Circuit | Count | Nations and IDs |
|---|---:|---|
| **Asia** | 20 | India `nation_ind` · Pakistan `nation_pak` · Bangladesh `nation_bgd` · Sri Lanka `nation_lka` · Nepal `nation_npl` · China `nation_chn` · Japan `nation_jpn` · South Korea `nation_kor` · Indonesia `nation_idn` · Vietnam `nation_vnm` · Thailand `nation_tha` · Philippines `nation_phl` · Malaysia `nation_mys` · Singapore `nation_sgp` · Kazakhstan `nation_kaz` · Uzbekistan `nation_uzb` · Saudi Arabia `nation_sau` · United Arab Emirates `nation_are` · Iran `nation_irn` · Israel `nation_isr` |
| **Europe** | 12 | United Kingdom `nation_gbr` · France `nation_fra` · Germany `nation_deu` · Italy `nation_ita` · Spain `nation_esp` · Netherlands `nation_nld` · Poland `nation_pol` · Sweden `nation_swe` · Switzerland `nation_che` · Russia `nation_rus` · Ukraine `nation_ukr` · Türkiye `nation_tur` |
| **Africa** | 9 | Egypt `nation_egy` · Nigeria `nation_nga` · South Africa `nation_zaf` · Kenya `nation_ken` · Ethiopia `nation_eth` · Morocco `nation_mar` · Ghana `nation_gha` · Tanzania `nation_tza` · DR Congo `nation_cod` |
| **Americas** | 7 | United States `nation_usa` · Canada `nation_can` · Mexico `nation_mex` · Brazil `nation_bra` · Argentina `nation_arg` · Colombia `nation_col` · Chile `nation_chl` |
| **Oceania** | 2 | Australia `nation_aus` · New Zealand `nation_nzl` |

Türkiye and Russia compete in the Europe circuit, and Egypt in Africa. Oceania's two nations make a thin field, so the owner may choose to merge it with Asia into an Asia-Pacific circuit.

**Starting data is authored at M3-10:** each nation's Country Level, band, cohorts, aggregates and 3–8 notables. Only India's starting band is fixed here: Developing (§2).
