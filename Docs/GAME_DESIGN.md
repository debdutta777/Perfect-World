# Perfect World: Awakening — Game Design

**Document status:** production plan and target design. It describes the intended game and vertical slice. A code scaffold may demonstrate data structures, validation, saving, or a small playable room; that scaffold is not evidence that the systems below are content-complete.

## Product statement

*Perfect World: Awakening* is a single-player desktop life simulation and action RPG built in C++ with Unreal Engine. The player begins at age sixteen in Class XI in Kolkata, West Bengal, in the fictional neighborhood of Nabapara. School, money, family obligations, friendships, health, and time matter before and after a supernatural Awakening. What the player repeatedly does shapes later abilities; the creator never sells a combat class or supernatural power.

Windows is the initial development and test target with keyboard/mouse and controller. The original mobile-first intent remains: Android packaging and real-device performance are early gates, with touch UI designed from the start. iOS, macOS, Linux and consoles are later feasibility work, not promised ports. India is the only authored country initially; one Kolkata neighborhood is the first playable scope.

The emotional promise is a gradual change from an ordinary teenager with a timetable and household responsibilities into an unusual person whose choices attract attention. Power does not erase ordinary life. It makes every existing obligation harder to balance.

## Design pillars

1. **A life before a build.** The player studies, travels, eats, rests, helps at home, earns small amounts, and forms relationships. Repeated conduct becomes the input to supernatural growth.
2. **Time makes choices legible.** A day has room for only a few meaningful commitments. Travel, attendance, recovery, and family expectations create understandable opportunity costs.
3. **One district reacts.** Shops open, buses run, rain changes foot traffic, schools close around incidents, and people remember visible actions. Density and consequence matter more than acreage.
4. **Power emerges.** Attributes, learned skills, Tattva affinities, equipment habits, and narrative decisions combine into generated paths. The player can steer development without selecting Warrior, Mage, or Assassin.
5. **India is daily reality, not decoration.** School structure, households, money, food, languages, transport, public institutions, monsoon behavior, and social expectations are specific and internally consistent.

Every feature must connect at least two of ordinary life, relationships, city systems, and the supernatural. A standalone system with no such connection is deferred.

## Player and lifecycle

The opening date is the first Monday of Class XI. The player is exactly 16 on opening day; any birthday choice must preserve this starting age. They live with a parent, extended family member, or appointed guardian; a minor cannot begin alone in a rented flat. The creator defines circumstances and modest starting tendencies, not destiny. Full rules are in `CHARACTER_CREATION.md`.

The long arc is planned as five lifecycle bands:

| Band | Typical age | Main pressures | Planned progression |
|---|---:|---|---|
| Higher secondary | 16–18 | attendance, boards, guardian trust, first income | slice and first release |
| Training/college/entry work | 18–22 | admissions, fees, commute, independence | post-launch expansion |
| Early career | 21–30 | qualifications, rent, career, partnerships | future city chapters |
| Established adult | 28–45 | leadership, household, organizations | long-term plan |
| Later life/legacy | 45+ | mentorship, health, succession, public impact | design reserve |

Age is calendar-driven, never a level. The vertical slice spans nine in-game days and does not reach a birthday. The first full release is designed to reach the end of Class XII; later lifecycle bands require their own content gates and are not implied by the scaffold.

## Core simulation

### Clock and planning

The default free-roam scale is one real second to one game minute. Dialogue, menus, accessibility pause, and tactical Gate choices can pause or slow the clock. Long activities resolve through authored interactions and then advance time: a school period advances 45 minutes, focused study 30–120 minutes, and sleep 4–10 hours. Gates use a separate mission clock while the city advances at a capped rate so a ten-minute mission cannot consume an implausible school week.

Each morning the phone shows fixed commitments, travel estimates, household requests, weather, and optional opportunities. There is no daily-login reward or real-time energy timer. Missing an opportunity changes the world state or lets it expire; it never removes a unique build permanently without explicit warning.

### Needs and condition

Four readable meters support decisions without turning the game into survival micromanagement:

| Meter | Normal sources | Low-state consequence | Recovery |
|---|---|---|---|
| Energy | exertion, late nights, combat | slower learning and stamina recovery | sleep, rest, food |
| Hunger | elapsed time, activity | reduced energy ceiling; irritability cues | meals and snacks |
| Health | injury, illness, untreated strain | activity restrictions and absence | treatment and time |
| Stress | exams, conflict, danger, overcommitment | narrower dialogue composure and slower recovery | rest, support, recreation |

Needs modify performance by bounded percentages; they do not kill a healthy player through ordinary neglect. Severe injury creates cost, absence, and relationship scenes rather than permadeath.

### Attributes and skills

Six attributes describe current capability: Body, Agility, Intelligence, Perception, Willpower, and Charisma. Starting traits add small bonuses, capped at roughly one normal week of early progression. Attributes grow slowly through repeated relevant behavior and major milestones.

Skills represent learned knowledge and remain separate from attributes. The first slice tracks Academics, Fitness, Computing, Practical Repair, Communication, and Tattva Studies. The full plan expands into mathematics, sciences, medicine, commerce, cooking, arts, music, driving, mechanics, psychology, history, and mythology. A high-Intelligence character still cannot repair a motor without Practical Repair experience.

### Education

The player attends Nabapara Vidyalaya in Class XI. At creation they choose a broad school stream—Science, Commerce, or Humanities—which changes timetable, known classmates, and early skill opportunities. It does not lock a career or Awakening path. Stream changes are possible through a counselor story before the term deadline and carry believable catch-up work.

School play uses short decisions, observation, dialogue, and optional subject challenges rather than simulated lectures. Attendance, coursework, tests, teacher trust, and peer collaboration are tracked. The slice includes two school days, one class interaction, one lab or group activity, a missed-class consequence, and a teacher follow-up. Board exams, admissions, vocational certificates, university, and professional licensing belong to later releases.

### Household and housing

Every starting home has a guardian NPC, sleeping space, study surface, storage, kitchen access, curfew expectation, chores, and household budget state. Family resources affect convenience and pressure, while guardian support affects communication and recovery; neither measures love or moral worth. The slice offers three authored home layouts using shared modular art: compact family flat, extended-family row house, and staff quarters/rented room attached to a guardian household.

Housing progression begins only after adulthood. Hostel, shared flat, rental, ownership, furnishing, and household formation are lifecycle systems for later chapters. Furniture bonuses remain small and practical: a good desk reduces study fatigue; it never grants Intelligence.

### Money and work

All normal-world prices use ₹. The slice economy targets meaningful small choices: chai ₹15–25, bus ₹15–30, a simple meal ₹70–140, stationery ₹20–200, clinic consultation ₹300–800, and a budget phone repair ₹800–2,500. Prices are fictionalized and tuned for play, then reviewed for regional plausibility.

At sixteen, legal and family constraints matter. The available earning activities are tutoring a younger pupil, a supervised shop shift, a school-approved event job, and small digital or repair commissions. Hazardous formal Gate work is unavailable. The first Gate incident is accidental; the first voluntary entry is supervised and story-authorized. Adult careers—engineering, medicine, government service, research, skilled trades, business ownership, and licensed hunting—are planned lifecycle content.

### Relationships and reputation

Named NPCs track familiarity, trust, strain, obligation, and shared memories. Labels such as Acquaintance, Friend, Close Friend, Trusted, Rival, and Estranged summarize state but do not replace it. Gifts can contextualize a scene; repeated gifts cannot grind affection. The teenage slice permits age-appropriate crushes and boundaries but contains no explicit sexual content and no adult/minor romance.

Reputation is audience-specific: School, Neighborhood, Gate Authority, Research Community, Hunter Community, Public, Corporate, and Underworld. Only the first four appear in the slice. A filmed rescue may raise Public attention while reducing Guardian trust because the player ignored instructions.

## Supernatural systems

### Gates

Gates are spatial discontinuities that society has adapted around through alerts, cordons, research, licensed response, insurance, rumor, and opportunism. Each instance has rank, Tattva bias, stability, environment, objective, inhabitants, modifiers, and collapse window. Initial play uses F, E, and an anomalous E-to-D escalation. Higher ranks exist in fiction, not as slice content.

The reusable mission grammar is Entrance → Read/Choose → Encounter → Complication → Elite/Boss → Core/Exit. A run lasts 6–12 minutes in the slice. Objectives include rescue, retrieval, stabilization, observation, escape, and combat. Modifiers such as flooded floor, low visibility, unstable geometry, heat, or time shear change routes and tactics; they do not merely multiply enemy health.

### Tattvas and emergent paths

Five affinities organize effects:

- **Prithvi:** structure, stability, weight, protection.
- **Jala:** adaptation, flow, restoration, cleansing.
- **Agni:** transformation, heat, release, destruction.
- **Vayu:** motion, speed, pressure, redirection.
- **Akasha:** space, perception, relation, rare discontinuity.

The game observes skill use, attributes, risk behavior, tool preference, relationships, and decisions. At authored thresholds it offers discoveries consistent with that history. A player strong in Computing and Practical Repair who stabilizes space with devices may discover a path called Void Engineer; that name is a readable summary of earned capabilities, not a selected class. Paths can overlap, change, and remain unnamed. Character-creation points cannot buy Tattva affinity, powers, Gate rank, equipment, or a path.

### Combat

The desktop control set supports move, camera/aim, light action, heavy/context action, dodge, two equipped abilities, utility, interact, and pause. Controller parity is mandatory. The slice has three weapon families—staff/baton, short blade/tool, and improvised ranged artifact—plus unarmed avoidance. Encounters emphasize tells, positioning, environmental use, and rescue priorities. Difficulty options change timing, assistance, and damage tolerance without withholding story.

The first boss, the Broken Guardian, has three mechanics: telegraphed sweeps, breakable arena anchors, and a stability phase where fighting alone fails. The player must read the space and use an environmental stabilizer taught earlier. A no-damage clear is optional; understanding is required.

## Opening arc: The First Door

The revised opening is school-centered:

| Time | Event and purpose |
|---|---|
| 0:00–2:00 | Wake in the selected guardian home; learn movement, phone, and one household interaction. |
| 2:00–5:00 | Travel to school by foot/bus; buy or skip breakfast; meet Ishan or Meera at Shanti Chai Corner. |
| 5:00–8:00 | Class XI assembly and stream activity; establish timetable, teacher, and ordinary stakes. |
| 8:00–10:00 | A routine distant Gate alert barely interrupts school, showing that Gates are normalized. |
| 10:00–15:00 | After-school choice: study, help guardian, train, or meet a friend. The choice seeds later dialogue. |
| 15:00–20:00 | Bus diversion and local power flicker; an E-rank containment zone expands around the player. |
| 20:00–30:00 | Accidental Gate survival tutorial. The player has no powers and must observe, evade, assist, and improvise. |
| 30:00–40:00 | Broken Guardian encounter; an unclassified Awakening event enables escape, not domination. |
| 40:00–50:00 | Clinic, guardian reaction, school absence, and Gate Authority interview. Ordinary consequences return. |
| 50:00–60:00 | First supervised test/training space and a choice about the strange Earth-origin artifact. |

Days 2–9 open gradually: recovery, school return, relationship repair, controlled training, a supervised voluntary Gate, artifact discovery, then free scheduling within the district. The long mystery is that some Gates contain recent objects from incompatible versions of Earth. “Perfect World” names an attempted ideal reality whose discarded alternatives may be the Gate environments.

## Vertical slice content contract

The slice is considered complete only when a fresh player can finish a representative 45–60 minute path and continue into one repeatable day. Target content:

| Area | Acceptance target |
|---|---|
| District | 0.35–0.5 km² dense authored space; key locations 30–90 seconds apart |
| Interiors | 3 home variants, school, chai stall, clinic, gym, Gate Authority lobby/training room |
| Named NPCs | 8, each with schedule, one relationship memory, and at least two reactive scenes |
| Crowd NPCs | 12 visual archetypes with schedule pools and conservative density settings |
| Story | 45–60 min first path; 9 in-game days available; 3 consequential opening choices |
| Gate spaces | 2 kits producing 3 authored layouts; one tutorial incident and one repeatable contract |
| Enemies | 4 behavior families, 6 visual variants, 1 boss |
| Progression | 6 life skills, 6 attributes, 5 affinity records, 8 discoverable ability prototypes |
| Equipment | 3 weapon families, 12 wearable/utility items, 6 food items |
| Simulation | clock, four needs, money, school attendance, weather states, NPC schedules, saving |
| Accessibility | remapping, controller, subtitles, scalable text, hold/toggle choices, aim assistance, difficulty |
| Performance | 60 fps target at 1080p High on the reference PC; 30 fps minimum supported profile |

Out of slice: whole-country travel, university, adult careers, home ownership, romance systems beyond age-appropriate narrative beats, multiplayer, live service, procedural city generation, drivable cars, S-rank Gates, company ownership, and a complete lifelong aging simulation.

## District layout and encounter flow

The launch district, Nabapara, is built around a transit spine:

```text
Maitri Staff Quarters ─ Nabapara Vidyalaya ─ Shanti Chai Corner
          │                         │                         │
     Clinic Lane ─────────── Banyan Maidan ───────── Old Market
          │                         │                         │
     Family Flats ───────── Kolkata Bus Hub ─── Commercial Row
                                    │
                           Authority Annex / Canal Works
                                    │
                           Closed Industrial Yard
```

The school, home, chai corner, and bus hub form the safe everyday loop. The Annex and industrial yard introduce institutional and supernatural pressure. Gates temporarily reroute crowds and buses, changing familiar routes without rebuilding the map.

## Art and audio direction

The normal world uses stylized 3D geometry, hand-authored materials, warm dust, painted signs, concrete repairs, vegetation, electrical clutter, and wet monsoon surfaces. Character proportions are lightly stylized but faces, skin tones, hair, uniforms, and clothing draw from contemporary Indian references. Avoid generic “exotic” ornament, pan-Indian collage, religious shorthand, or costume-like everyday dress.

Gate spaces use the same material logic under impossible rules: familiar brick and plaster, school tile, reinforcement bar, water tanks, or bus signage may repeat at wrong scale or gravity. Tattva color supports form and motion rather than replacing it: Prithvi is mass and layered planes; Jala is refraction and continuous curves; Agni is emissive fracture and transformation; Vayu is displaced particles and pressure ribbons; Akasha is parallax failure and negative space.

Portraits are 2D illustrated busts generated from approved model turnarounds. UI resembles a contemporary midrange phone but remains platform-neutral and fully operable by controller. Diegetic type uses Bengali-script and Latin scripts where appropriate; gameplay information is localized separately.

Audio layers ceiling fans, pressure cookers, school bells, buses, two-wheelers, vendors, rain on sheet roofs, neighborhood conversations, and distant construction without making every street continuously loud. Music blends contemporary orchestration and regionally reviewed instrumentation; it must not reduce a diverse region to a single “Indian” motif. Gates remove or misalign familiar ambience before adding supernatural sound.

## Save, architecture, and portability principles

- Save data stores stable IDs and versioned records, never raw object paths as the only identity.
- Simulation systems publish events through interfaces; UI reads view models rather than owning game state.
- Content values live in data assets/tables with validation. C++ owns rules and persistence; Blueprints assemble content and presentation.
- Input uses Unreal Enhanced Input actions with keyboard/mouse and controller mappings from day one.
- Platform services sit behind interfaces. The first build must run without a network account.
- Text is externalized and UI supports safe zones and scaling. No platform-specific font assumptions.
- Deterministic seeds are recorded for repeatable Gate layouts; core progress never depends on online randomness.
- Character-creation validation is authoritative in C++ and reused by UI, save loading, and automated tests.

## Success measures

For internal playtests, at least 80% of first-time players should complete creation without facilitator help; 90% should understand that traits are starting circumstances, not classes; 75% should name one ordinary obligation they deliberately sacrificed for another; and 70% should correctly explain why their first supernatural discovery occurred. The first-hour median should include at least three noncombat interactions and no more than twelve minutes of uninterrupted exposition.

The slice passes production review when all critical paths can be completed with keyboard/mouse and controller, saves survive schema migration, no creator preset breaks the 1,000-point rule, the opening maintains 60 fps on the reference PC in its busiest school scene, and external cultural review finds no high-severity setting errors.
