# Character Creation Specification

**Document status:** authoritative design contract for the planned creator. The accompanying scaffold implements the point ledger, trait definitions, validation, one default preset, and in-memory creation records. The other presets, trait effects and complete creator interface below remain planned. It does not imply that every appearance control, household scene, or trait effect is already playable.

**Updated 2026-09-23:** the appearance editor's development-time display is now block characters (voxel cuboids on the standard Unreal humanoid skeleton), not the 2D/2.5D anime-styled art this document described on 2026-09-20. See "Appearance editor" below and `GAME_PLAN.md` §3. The point ledger, trait catalogue, household rules and validation contract below are unaffected.

## Goals

Creation answers three questions: who the teenager is, what their current household and history look like, and which modest advantages they bring into the first school day. It must not answer what supernatural class they become. Identity and cosmetic expression are free. Exactly 1,000 background points are allocated across twenty traits; point prices rise faster than benefits so broad, believable histories remain competitive with specialization.

Completion targets are under eight minutes with a preset and under twenty minutes from scratch. Every mechanical control has plain-language impact text and a preview of affected starting values. The creator is entirely usable by controller.

## Player age and safeguarding rules

- The story begins in Class XI. The opening age is exactly 16. Any future birthday selector must preserve that age on opening day.
- The selected date of birth becomes canonical save data and drives later aging. Age is never adjusted to optimize stats.
- The player lives with a parent, relative, adult sibling aged 21+, or appointed guardian — or, if Household Resources is left at Rank 0, grew up in institutional care (a children's home) with no family guardian at all. See "Household selection" below for the 2026-09-20 decision that introduced this. “Lives alone” is unavailable while the character is a minor.
- School stream, household, and guardian affect scenes and schedules. They do not grant a purchased class or power.
- Teenage relationships are age-appropriate. Adult/minor romance, explicit sexual content, gambling, alcohol use, and hazardous employment are unavailable.
- Formal Gate licenses and unrestricted contracts are adult systems. The opening Gate is an accident; later slice access is supervised.

## Creation flow

1. **Identity:** name, pronouns, address terms, voice presentation, age/birthday, and optional accessibility settings.
2. **Appearance:** body, face, skin, hair, clothing, and presentation. All choices are cosmetic and cost zero.
3. **Household:** select an authored guardian relationship and home template (Household Resources Ranks 1–4), or leave Household Resources at Rank 0 for institutional care with no family template; edit names and relationship terms where applicable.
4. **School:** choose Science, Commerce, or Humanities and a reason for the choice. This sets a timetable, not a class.
5. **Background traits:** allocate exactly 1,000 points among twenty ranked traits or select a valid preset.
6. **Review:** show the first-day household, timetable, all mechanical starting effects, budget arithmetic, and content notes.
7. **Confirm:** write a versioned immutable creation record and start the prologue. The player may freely undo and refund until this step.

The player can move backward without losing edits. “Randomize appearance” never changes identity, household, school, or point allocation. “Randomize background” generates only valid 1,000-point allocations and shows the changed traits before acceptance.

## Free identity and cosmetic choices

The following never consume background points and do not alter stats, prices, dialogue success, or NPC attraction:

- Given name, family name, nickname, pronouns, and common relationship address terms.
- Voice set and pitch within recording-safe ranges.
- Gender identity and presentation; no gameplay is gated by a binary sex choice.
- Skin tone, facial structure, eyes, brows, nose, mouth, ears, marks, freckles, and non-graphic scars.
- Hair texture, style, color, facial hair where age-appropriate, head coverings, and religious/cultural accessories chosen respectfully.
- Body frame, height within animation-safe bounds, posture, handedness, and assistive-device presentation supported by the content set.
- School-uniform fit and permitted variants, casual outfit palette, jewelry, glasses, bag, and shoes.

Appearance has animation compatibility constraints, not “good/bad” values. Physical Presence is a learned bearing and first-impression trait; it is not a hidden attractiveness score. Personal Presentation reflects preparation and clothing care, not price, complexion, body type, gender conformity, or brand ownership.

### Appearance editor (design decision, 2026-09-20; scope corrected 2026-09-20; display updated 2026-09-23)

**Updated 2026-09-23.** The owner's voxel-block decision (`GAME_PLAN.md` §3) supersedes the "anime-styled 2D/2.5D characters" description below as the development-time appearance editor. During development the appearance step is a block character editor — a character built from cuboid parts on the standard Unreal humanoid skeleton, shown in an orbit camera (`GAME_PLAN.md` §4, Phase 1) — not a fixed set of pre-made heads, and not layered 2D/2.5D art. The final stylised 3D appearance editor arrives at milestone M9, per `GAME_PLAN.md` §8 (M9). The control split immediately below — what is a slider versus a discrete pick — is unchanged by this update; only the underlying art (block cuboids now, final stylised art later) changed, and the reasoning for the split is restated in those terms:

- **Continuous sliders — colours only, in the first version.** Skin tone, hair colour, and eye colour are true sliders (e.g. hue/value ranges or a calibrated ramp position), because a block or final material can be recoloured/tinted cheaply at runtime.
- **Discrete pickers — everything else, in the first version.** Hair style, face shape, build, and uniform/outfit variant are chosen from a finite set of authored options, each a distinct block part or (later) mesh variant, not a slider. Proportions and facial-feature sliders (height, head/limb scale, eye shape/spacing, nose, mouth, jaw, brow, ear morphs) are **deferred, not implemented during development**, and this is a deliberate scope decision, not an oversight: continuous proportion control needs morph targets on a skinned mesh to be cheap, and block cuboid parts do not have those. Continuous proportion control returns once the final stylised 3D presentation layer (`ASSET_PIPELINE.md`) exists at M9.

This extends rather than reduces the free-cosmetics list above; every slider and picker in the editor stays inside that list and stays free.

**Critical rule: appearance is stored as data, not as a picked asset.** The editor's output — slider values for colour, and selected option IDs for every discrete pick — is a named, versioned, engine-neutral parameter set, never a chosen mesh filename or reference. The same saved `AppearanceRecord` parameter set must be able to drive a block character (today's presentation layer, during development) and, later, the final stylised skinned mesh with morph targets (from M9; see `ASSET_PIPELINE.md`) without the save becoming invalid or needing a new schema. Presentation code reads the parameter set and decides how to render it; it never writes gameplay-visible state back into it. This engine-neutral, versioned shape is exactly what makes it safe to widen the discrete/continuous split later (e.g. adding proportion sliders once the final art lands) without another save-breaking schema change. **This `AppearanceRecord` shape is itself unchanged by the 2026-09-23 decision** — only which presentation layer renders it changed.

The existing rule stands unchanged: appearance parameters never cost creation points and must never alter point costs, stats, dialogue success, NPC attraction, or supernatural potential. A future slider/picker count or default preset for the editor is production work, not a design reopening of this rule.

## Household selection

**Implemented:** the five templates below exist as data in the portable core with their stable IDs (`parent_household`, `extended_family`, `grandparent_household`, `adult_sibling_household`, `appointed_guardian`), each naming a home kit ID. Selecting one costs no creation points, and the choice is locked once the prologue starts, the same as identity and cosmetics. The chosen household ID is part of the persisted creation record. **Planned, not yet built:** guardian records (names, individual relationship terms), editable relationship terms beyond the template default, the home scenes themselves, per-household art, and the institutional-care outcome described below — none of the six possible starting rooms exist yet as art or a level.

### Design decision, 2026-09-20 — Household Resources Rank 0 means institutional care

This reverses the rule this document stated before today. Household Resources Rank 0 no longer means an ordinary family baseline. It means the character grew up in **institutional care — a children's home — with no family household at all.** None of the five templates below applies at Rank 0. Household Resources Ranks 1 through 4 select one of the five templates and escalate the resources available inside it, at the reserve values already given in the trait catalogue (₹500 / ₹1,000 / ₹2,000 / ₹4,000).

The owner has made this decision deliberately, accepting a consequence that the earlier design contract was specifically written to avoid: **the point ledger and the origin's hardship are now coupled for this one trait.** A player who leaves Household Resources at Rank 0 in order to spend those 0 points elsewhere is knowingly choosing a harder starting origin, not a neutral one — spending fewer points now produces a harder origin. The sentence this replaces read: "Rank 0 means an ordinary baseline, never abuse, homelessness, or deprivation. Serious family harm belongs only in authored, reviewed stories and is not a point optimization choice." That protection is withdrawn for Household Resources Rank 0 only, as of 2026-09-20; it still holds for every other trait and every other rank, and no other trait may be spent (or left unspent) to buy or avoid family hardship. This paragraph exists so a future reader understands the coupling was chosen on this date, not an oversight the validator failed to catch.

**Content-handling note.** A children's-home origin must be written and depicted with dignity. It is not squalor, not abuse, and not a punishment for how a player allocated points — it is a sixth legitimate starting circumstance alongside the five templates below. Like every other household outcome, it must not grant or deny supernatural potential.

Five narrative templates, selected at Household Resources Rank 1 and above, share the same baseline access to a bed, meals, storage, school, and a guardian:

| Template | Guardian examples | Home kit | Story emphasis |
|---|---|---|---|
| Parent household | one or two parents/stepparents | compact family flat | work schedules and expectations |
| Extended family | aunt/uncle or multigenerational adults | row house | shared space and many obligations |
| Grandparent household | one or two grandparents | older cooperative flat | care flows in both directions |
| Adult sibling household | sibling aged 21+ | rented flat | young household under financial pressure |
| Appointed guardian | family friend/hostel warden | staff quarters with private sleeping space | negotiated trust and formal responsibility |

At Rank 0, the sixth outcome is institutional care: no template, no family guardian, a children's-home staff caregiver in their place.

This branch **is implemented** in the portable core as of commit `c081998`. `CharacterCreator::ResolveOpeningHome()` combines the chosen template with the Household Resources rank and returns an `OpeningHomePlan`. At Rank 0 it sets `GrewUpInCare`, overrides the home kit to `childrens_home_dormitory` and names the dressing tier `institutional_care`; at Ranks 1–4 it returns the chosen template's own home kit with the tiers `modest`, `steady`, `comfortable` and `ample`. The plan keeps reporting the template the player chose even while care overrides it, so raising the rank restores their choice instead of discarding it. The creator reads this resolved plan rather than a household's own kit, so the interface cannot contradict the rules at Rank 0.

What does **not** exist yet is any actual room: no home kit ID names a built level or mesh set, and confirming an origin still loads no scene. Building the rooms is future work — see `Docs/ROADMAP.md`.

Household Resources, Guardian Support, and Housing Stability are purchased separately so money, care, and security are never treated as the same thing. A low-resource home (Ranks 1+) can be highly supportive; a wealthy home can be emotionally distant. This baseline-versus-support independence is unchanged above Rank 0; only Rank 0 itself changed meaning today.

## Point economy

Each of the twenty traits has ranks 0–4. The displayed cost is cumulative:

| Rank | Cumulative cost | Increment from prior rank | Design meaning |
|---:|---:|---:|---|
| 0 | 0 | — | ordinary baseline* |
| 1 | 25 | 25 | noticeable tendency |
| 2 | 60 | 35 | practiced advantage |
| 3 | 110 | 50 | strong part of the background |
| 4 | 180 | 70 | exceptional starting circumstance |

\* "Ordinary baseline" describes Rank 0 for nineteen of the twenty traits. As of 2026-09-20, Household Resources is the sole exception: its Rank 0 means institutional care (a children's home), not an ordinary baseline. See "Household selection" above.

The player must spend **exactly 1,000 points**. No more than four traits may be Rank 4. There are no negative ranks and no disadvantage traits that award points. Increasing a trait charges only the difference to its new cumulative price; lowering it refunds only the difference originally committed in the current creator session.

Point operations occur in a transient draft ledger. Undo/redo restores prior drafts. Points cannot become currency, XP, items, or an account balance; saving, reloading, toggling presets, or repeated undo cannot create value. On confirmation the ledger is consumed and only ranks plus the creation schema version are stored. Post-prologue progression never refunds creator points. If a future patch materially changes a trait, migration maps old ranks to equivalent starting state or grants a one-time full creator rebuild from the original 1,000—not incremental refunds.

The cost curve is intentionally nonlinear. Rank 4 costs 7.2 times Rank 1 but provides roughly four times its small numeric benefit. Specialization is expressive, not the optimal source of raw power.

## Canonical trait catalogue

IDs are save/API contracts and must not be renamed without migration. Numeric effects below are starting modifiers at R1/R2/R3/R4. They stack additively within the creator and then obey the relevant system cap. Rank 0 has no penalty. One ordinary early activity grants about 20–30 skill XP, which gives scale to the XP packages.

| Domain | Stable ID | Display name | Starting effect at R1 / R2 / R3 / R4 |
|---|---|---|---|
| Appearance & presentation | `physical_presence` | Physical Presence | +1 / +2 / +3 / +4% composure window in first-impression interactions |
| Appearance & presentation | `personal_presentation` | Personal Presentation | +1 / +2 / +3 / +4% reduction to stress gained from formal school/social checks |
| Family resources & support | `household_resources` | Household Resources | R0: institutional care (children's home), no family template, no reserve; R1/R2/R3/R4: chosen household template with discretionary reserve +₹500 / ₹1,000 / ₹2,000 / ₹4,000; not player cash |
| Family resources & support | `guardian_support` | Guardian Support | +1 / +2 / +3 / +4% stress recovery from supportive home interactions |
| Family resources & support | `housing_stability` | Housing Stability | +1 / +2 / +3 / +4% sleep recovery at the starting home |
| Physical | `vitality` | Vitality | +1 / +2 / +3 / +4% maximum Health at start |
| Physical | `strength` | Strength | +1 / +2 / +3 / +4% carry capacity and force-task tolerance |
| Physical | `agility` | Agility | +1 / +2 / +3 / +4% maximum exertion stamina at start |
| Cognition | `analytical_reasoning` | Analytical Reasoning | +1 / +2 / +3 / +4% progress from logic/problem-solving activities |
| Cognition | `learning_discipline` | Learning Discipline | +1 / +2 / +3 / +4% focused-study efficiency before fatigue |
| Cognition | `perception` | Perception | +1 / +2 / +3 / +4% generous timing on observation prompts |
| Talents | `technical_aptitude` | Technical Aptitude | Computing and Practical Repair each +10 / +25 / +45 / +70 starting XP |
| Talents | `creative_aptitude` | Creative Aptitude | Communication plus future Creative skill +10 / +25 / +45 / +70 starting XP |
| Talents | `athletic_aptitude` | Athletic Aptitude | Fitness +10 / +25 / +45 / +70 starting XP |
| Social | `empathy` | Empathy | +1 / +2 / +3 / +4% generous timing on emotion-reading prompts |
| Social | `confidence` | Confidence | +1 / +2 / +3 / +4% reduction to stress from public speaking and assertion |
| Social | `community_connections` | Community Connections | start with 1 / 2 / 3 / 4 neighborhood contact leads; leads are introductions, not automatic friends |
| Prior experiences | `academic_foundation` | Academic Foundation | Academics +10 / +25 / +45 / +70 starting XP |
| Prior experiences | `practical_experience` | Practical Experience | Practical Repair and household-task familiarity +10 / +25 / +45 / +70 starting XP |
| Prior experiences | `adversity_resilience` | Adversity Resilience | +1 / +2 / +3 / +4% recovery from noncombat setbacks after rest/support |

### Effect guardrails

- Trait ranks affect starting state or bounded learning/recovery rates. They never grant dialogue auto-success, a qualification, a relationship, wealth owned by the player, a weapon, Tattva affinity, or supernatural ability.
- Similar modifiers use the highest applicable prompt accommodation plus at most one general modifier; they do not multiply into large bonuses.
- Contact leads unlock introductions only after an in-world action. They cannot be sold or repeatedly harvested.
- Household reserve pays authored household needs and occasional support decisions. It is not withdrawable cash and does not replenish through reloads.
- All XP packages are applied once when a new game state is created, recorded by a creation transaction ID, and covered by idempotency tests.
- Creator advantages remain useful but can be surpassed through play. Approximately two to four focused early sessions should close any Rank 4 starting skill-XP gap.

## Verified presets

Presets allocate identity-neutral backgrounds. They never change appearance, household template, stream, or difficulty. All unlisted traits are Rank 0. Selecting a preset replaces the draft allocation and remains fully editable.

### Balanced Scholar — canonical default

- Rank 3 (4): `guardian_support`, `learning_discipline`, `empathy`, `academic_foundation`
- Rank 2 (6): `housing_stability`, `vitality`, `agility`, `analytical_reasoning`, `technical_aptitude`, `adversity_resilience`
- Rank 1 (8): `physical_presence`, `personal_presentation`, `household_resources`, `strength`, `perception`, `creative_aptitude`, `confidence`, `practical_experience`
- Rank 0 (2): `athletic_aptitude`, `community_connections`

Arithmetic: **4×110 + 6×60 + 8×25 = 440 + 360 + 200 = 1,000**. Rank-4 count: 0.

### Resourceful Maker

- Rank 4 (1): `practical_experience`
- Rank 3 (4): `technical_aptitude`, `analytical_reasoning`, `perception`, `adversity_resilience`
- Rank 2 (3): `guardian_support`, `strength`, `community_connections`
- Rank 1 (8): `personal_presentation`, `household_resources`, `housing_stability`, `vitality`, `agility`, `learning_discipline`, `empathy`, `confidence`
- Rank 0 (4): `physical_presence`, `creative_aptitude`, `athletic_aptitude`, `academic_foundation`

Arithmetic: **1×180 + 4×110 + 3×60 + 8×25 = 180 + 440 + 180 + 200 = 1,000**. Rank-4 count: 1.

### Athletic Organizer

- Rank 4 (1): `athletic_aptitude`
- Rank 3 (4): `agility`, `vitality`, `confidence`, `community_connections`
- Rank 2 (3): `strength`, `guardian_support`, `empathy`
- Rank 1 (8): `physical_presence`, `personal_presentation`, `housing_stability`, `perception`, `learning_discipline`, `practical_experience`, `adversity_resilience`, `academic_foundation`
- Rank 0 (4): `household_resources`, `analytical_reasoning`, `technical_aptitude`, `creative_aptitude`

Arithmetic: **1×180 + 4×110 + 3×60 + 8×25 = 1,000**. Rank-4 count: 1.

### Creative Connector

- Rank 3 (4): `creative_aptitude`, `empathy`, `community_connections`, `personal_presentation`
- Rank 2 (6): `physical_presence`, `guardian_support`, `perception`, `learning_discipline`, `confidence`, `adversity_resilience`
- Rank 1 (8): `household_resources`, `housing_stability`, `vitality`, `agility`, `analytical_reasoning`, `technical_aptitude`, `academic_foundation`, `practical_experience`
- Rank 0 (2): `strength`, `athletic_aptitude`

Arithmetic: **4×110 + 6×60 + 8×25 = 1,000**. Rank-4 count: 0.

### Independent Generalist

- Rank 2 (10): `physical_presence`, `housing_stability`, `vitality`, `agility`, `analytical_reasoning`, `perception`, `technical_aptitude`, `empathy`, `practical_experience`, `adversity_resilience`
- Rank 1 (16 would exceed the 20-trait catalogue, so this composition is intentionally not used). The valid allocation below replaces it:
- Rank 3 (5): `housing_stability`, `vitality`, `analytical_reasoning`, `empathy`, `adversity_resilience`
- Rank 2 (5): `agility`, `perception`, `technical_aptitude`, `community_connections`, `practical_experience`
- Rank 1 (6): `physical_presence`, `personal_presentation`, `guardian_support`, `learning_discipline`, `confidence`, `academic_foundation`
- Rank 0 (4): `household_resources`, `strength`, `creative_aptitude`, `athletic_aptitude`

Arithmetic: **5×110 + 5×60 + 6×25 = 550 + 300 + 150 = 1,000**. Rank-4 count: 0.

**Note (2026-09-20):** this preset leaves `household_resources` at Rank 0. Under the household decision above, selecting Independent Generalist now means the character grew up in institutional care rather than in one of the five family templates. This is a deliberate consequence of the preset's existing point spread, not a new allocation choice; the preset's numbers are unchanged.

The UI must display only the valid Independent Generalist rows. The explanatory rejected composition above is retained in the design record to make the arithmetic review explicit; it is not shipped as UI copy or data.

## School stream and creator separation

Science, Commerce, and Humanities change the first timetable, course dialogue, and accessible school activities. Each stream begins at the same total Academics XP. Trait XP is then applied independently. A Science student with Creative Aptitude and low Technical Aptitude is valid; a Humanities student can later become an engineer by completing prerequisites. Stream choice does not alter the 1,000-point budget.

Difficulty and accessibility also sit outside the budget. Aim assistance, prompt duration, reading speed, subtitles, navigation hints, and combat damage settings never cost points or change achievement eligibility in the planned single-player game.

## Validation and persistence contract

Creation is valid when all conditions hold:

1. All twenty stable IDs occur exactly once.
2. Every rank is an integer from 0 through 4.
3. The sum of cumulative costs is exactly 1,000.
4. No more than four traits are Rank 4.
5. Identity, birthday, guardian template, school stream, and appearance seed/choices pass their independent validators.
6. No trait payload contains a class, affinity, power, equipment grant, or negative-point award.

The validator must reject unknown, duplicate, and missing IDs; never silently coerce an invalid imported save into a stronger build. UI confirmation stays disabled with a specific explanation. Presets are validated from the same data path at startup and in automated tests. The canonical default ID is `balanced_scholar`.

Recommended save record fields are `CreationSchemaVersion`, `CreationTransactionId`, `DateOfBirth`, `IdentityRecord`, `AppearanceRecord`, `HouseholdTemplateId`, `GuardianRecord`, `SchoolStreamId`, `TraitRanksById`, `SpentPoints`, and `StartingEffectsApplied`. `StartingEffectsApplied` and the transaction ID make one-time packages idempotent.

## UX acceptance checks

- A keyboard-only and controller-only player can complete every field, inspect every effect, undo, choose a preset, and confirm.
- Remaining points, next-rank incremental cost, cumulative trait cost, and Rank-4 count are visible on the trait screen.
- Attempting a fifth Rank 4 explains the cap before changing the ledger.
- Reducing a rank immediately restores the exact incremental difference; repeated undo/redo never changes total value.
- The review page shows a 1,000/1,000 total and lists all nonzero effects in plain language.
- Defaulting the entire creator yields the verified Balanced Scholar allocation and a valid 16-year-old Class XI student in a guardian household.
- No cosmetic option changes the mechanical preview, and automated snapshots compare resulting stats across cosmetic variants.

