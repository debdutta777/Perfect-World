# Systems catalogue

**Document status:** part of `SYSTEMS.md`, split out on 2026-09-23 for length. Every system, its state, commands, events, dependencies, save data, tests and milestone. `SYSTEMS.md` holds the principles, reducer, dependency graph, save architecture, budgets, testing and risks.

## 3. Systems

**Legend:** **built** = exists in code. Core = `PerfectWorldCore`; UE = the `PerfectWorld` module. *Save* names the snapshot section and its first version. *Tests* run in CTest unless stated. Each entry gives its milestones, then what the system does.

### 3.1 Foundation

**Game clock** — **built** (`GameClock.h`) · Core · M0. The one game-minute time, calendar conversion and pending-event queue.
- **Owns:** `GameClockSave{Now, NextHandle, Pending[ScheduledEvent{Handle, When, Id}]}`, with the epoch at Monday 2 June 2042, 06:00.
- **Must change:**
  1. It becomes owned by `WorldState` and is advanced only by the reducer.
  2. `ScheduledEvent` gains `int16 Phase`, and the queue sorts by (When, Phase, Handle). Phases: 0 calendar, 10 world events, 20 obligations, 30 activities, 40 threads, 90 receipts.
  3. Add `FireDue()`: `Advance(0)` currently returns before firing events due at `Now`.
  4. Midnight becomes a `calendar/midnight` event instead of the batched `DaysCrossed`, so rollover interleaves correctly during multi-day jumps.
  5. Insert in sorted position instead of re-sorting on every `Schedule` (the queue holds at most 5,000).
  6. Add a JSON codec; only a save struct exists today.
- **Save:** `clock` v1. **Tests:** the existing 16,857 assertions, plus phase order, scheduling inside the window, codec round trip and corruption.

**Command reducer** — Core · M1. Validates and applies every change, routes clock events and emits domain events (§2).
- **Owns:** `WorldMeta{LifeId, WorldSeed, ContentHash, NextSeq, NextInstance}`, the command-log ring and the wiring table.
- **Commands:** `time.continue`; `debug.*` (rejected in Shipping). **Reads:** everything, through system handlers.
- **Save:** `meta` v1. **Tests:** fuzzed command sequences against every invariant; a rejection leaves the hash unchanged; `Partial` plus `continue` equals one uncapped run.

**Deterministic RNG streams** — Core · M1. Named persisted streams plus keyed derivation.
- **Owns:** `RngBank{WorldSeed, map<StreamId, Pcg32{State, Inc, Draws}>}`.
  - Streams: `opportunity`, `tower` and `ambience` (from `MASTER_PLAN.md`), plus `school`, `events`, `competition` and `npc_life`.
  - `Rng.Keyed(stream, key...)` gives a PCG32 seeded by SplitMix64 of FNV-1a(WorldSeed, stream, key). Use it wherever a result must be re-derivable or independent of time granularity.
  - In-house helpers: `UniformInt` (Lemire), `ChancePermille`, `WeightedPick`.
- **Save:** `rng` v1 (hex strings). **Tests:** known-answer vectors, stream independence, and equal hashes from the CTest build and the UE-hosted build.

**World state and versioned save** — creation save **built** (schema v4) · codec in Core, file IO in UE · M1 (v5). The `WorldState` aggregate and its snapshot (§6).
- **Owns:** `SaveDocument{Header{format, schemaVersion, build, savedAtUtc, summary, recap, resume}, Meta, Hashes, Sections}` and the migration registry.
- **Must change:**
  - `CharacterCreationSave` becomes section `creation`, verbatim.
  - The hand-written reader grows into a JSON DOM with `int64`, arrays and full `\u` escapes. Today it reads `int32` only and rejects `\u` above 0x7F.
  - Slot code moves from the creator subsystem to `UPWSaveSubsystem`.
- **Tests:** a fixture for every version ever written (including creation v1–v4); truncation at every prefix; byte-flip fuzz; byte-identical round trip.

**Content registry and validator** — Core, `pwcontent` and an editor commandlet · M1. Loads `Data/Content` into an immutable `ContentDb` and refuses invalid content at boot.
- **Owns:** `ContentDb{typed tables map<Id, Def>, Aliases, ContentHash}`. Never saved; saves record only its hash.
- **Must change:** the compiled creation catalogue (traits, households, dressing tiers, appearance options) moves to `Data/Content/creation/`, with a parity test against the `constexpr` tables until they are deleted.
- **Tests:** every rule has a failing fixture in `Tests/Fixtures/content_bad/`. `pwcontent validate` runs in CTest.

**Asset registry and resolver contract** — ID checks in Core, resolution in UE · M1. Gameplay references asset IDs only. `ASSET_PIPELINE.md` owns the registry file, the ID grammar and the swap.
- **Owns (gameplay side):** `AssetRef{Id}` fields in content, and interaction profiles keyed by asset ID — anchors, verbs, seats, capacity, targeting priority (`SYSTEMS_DATA.md` §3.10).
- **Rule:** the core treats IDs as opaque. Collision, sockets and navigation modifiers belong to the registry entry; interaction anchors belong to gameplay data. The cross-layer validator checks that every anchor lies inside the registered bounds.
- **Tests:** every referenced ID resolves. The M9 coverage report reads the same registry.

### 3.2 Character

**Creation** — **built** (`CharacterCreator`) · Core · M0 verified; M2 identity, stream, review. The 1,000-point ledger, households, rank-0 care, the opening home.
- **Owns:** `CreationState{CharacterCreator Draft, bool Confirmed}`, frozen after confirmation.
- **Commands:** `creation.set_trait_rank`, `.set_household`, `.set_identity`, `.set_appearance`, `.apply_preset`, `.set_resume` and `.confirm{WorldSeed, LifeId}`. Each maps one-to-one onto an existing method, so the panels keep their Blueprint API through a façade.
- **Events:** `CreationConfirmed`. Confirmation:
  - seeds Stats from traits through `creation/trait_seeds.json` (about one week of ordinary progression);
  - creates the household and registers its family identities;
  - opens Money;
  - opens the calendar at 06:00 on 2 June 2042.
- **Must change:**
  - `IdentityProfile` gains `DateOfBirth` (3 June 2025 to 2 June 2026, so the character is exactly sixteen on opening day), `Languages[]`, `SchoolStreamId` and `StreamReason`.
  - `ResumePoint` gains `identity`, `stream` and `review`; existing text IDs are unchanged.
  - `StartLife` becomes `confirm`, made idempotent by `LifeId`.
- **Save:** `creation` v5 (v4 plus the new fields). **Tests:** the existing 225k assertions, migration of the v1–v4 fixtures, and the date-of-birth boundary.

**Attributes, skills and latent growth** — Core · M2; ageing M5. Six attributes, skills and methods, the rolling practice window, and "mistakes travel".
- **Owns:** `StatsState`:
  - `Attributes array<Milli,6>` — Body, Agility, Intelligence, Perception, Willpower, Charisma;
  - `Skills map<Id, {LevelMilli, Methods, ConsolidatedAt}>`;
  - `Practice ring<{Family, At, CreditMilli}>` over 24 hours, plus `Sessions` for three-in-seven consolidation;
  - `Evidence array<Milli,6>`;
  - `Misconceptions map<Concept, {Since, Family}>`.
- **Commands:** none; it applies the learning in `ActivityCompleted`. **Events:** `SkillLearned`, `MethodAcquired`, `TechniqueConsolidated`, `MisconceptionFormed` / `MisconceptionResolved`.
- **Reads:** `tuning/growth.json` — credit 1000/500/100/0; a per-skill daily budget; attribute growth at most one-twentieth of skill growth; ageing curves peaking at 22–32 physically and rising into the 50s cognitively.
- **Save:** `stats` v1. **Tests:** a single circled family plateaus; changing variants does not reset the window; no religion, language, family or stream multiplier exists (the validator rejects such tags).

**Condition** — Core · M2. Energy, hunger, health and stress: bounded, and never lethal from neglect.
- **Owns:** `ConditionState{Energy, Hunger, Health, Stress: Milli 0..100000; RatesPerMinute (integers set by the current activity); LastIntegratedAt; Injuries[{Id, Severity, HealsAt}]}`.
  - Neglect floors Health at 20,000 and triggers a care scene.
  - Death comes only from later-life ageing.
- **Events:** `ConditionThreshold{meter, band}`, predicted and scheduled so it fires at the exact minute.
- **Save:** `condition` v1. **Tests:** additivity; recovery curves; an exhaustive check that no neglect path reaches death.

**Awakening profile and abilities** — Core · M4. Rules, not an LLM, turn the lived record and dungeon conduct into a profile.
- **Owns:** `AwakeningState`:
  - `Tattva array<Milli,5>` summing to 100,000;
  - `Capability{Output, Precision, Range, Duration, Perception, Coordination}`, each independent;
  - `Abilities[{Id, OriginTag, Rank}]`;
  - `BehaviourLedger[{Tag, At, Weight}]`.
- **Events:** `Awakened` and `AbilityGained`, from an internal resolve on `DungeonCompleted`. **Reads:** Stats, activity tags, dungeon conduct, `awakening/*.json`.
- **Save:** `awakening` v1. **Tests:** every Tattva is reachable by several different lives; affinity never changes a ceiling; every output names its top three causes.

**Appearance** — record **built** (`AppearanceRecord` v1) · Core record, UE display · M0; M1 block parts; M5 changes during life. An engine-neutral look for the player and for every person.
- **Owns:** `AppearanceRecord{FaceShapeId, HairStyleId, BuildId, UniformVariantId, SkinTone, HairColor, EyeColor}`.
- **Must change:**
  - Options move to `creation/appearance_options.json`, each naming block-part asset IDs on the UE humanoid skeleton. The header comment about 2D sprites is out of date.
  - NPCs use the same record, generated by keyed RNG at crystallisation.
  - Record v2 (M5) adds `OutfitId` and an `appearance.change` command (barber, wardrobe) after life starts. Age shows through derived presentation, not by editing the record.
- **Save:** within `creation` for the player; within identity records for NPCs.

### 3.3 Life

**Calendar and time granularity** — Core · M2 day; M3 week, festivals; M4 countdown; M5 season, year; M8 life close. Owns clock modes (§2.5), plans, nominations and compression.
- **Owns:** `TimeControlState`:
  - `Mode`; `DayPlan[{Start, End, ActivityId|CommitmentId}]`; `WeekPlan`;
  - `StandingOrders[{Situation, Response}]`;
  - `Nominated[Day]` (at most 3 a week) and `InterruptsUsed` (at most 1 a week);
  - `FirstsSeen`;
  - `Mission?{Kind, MissionMinutes, RatePermille, CapMinutes|FixedMinutes}`;
  - almanac data (the 2042–43 festivals, verified by a reviewer).
- **Commands:** `time.advance`, `.plan_day`, `.plan_week`, `.set_order`, `.resolve_week|season|year`, `.drop_into`; `nomination.accept|decline`; `life.close` (the epilogue is a read-only report built from the histories).
- **Events:** `DayStarted`, `DayEnded`, `WeekResolved`, `SeasonStarted`, `YearStarted`, `SceneDayNominated`, `CompressionStopped{reason}`, `UnattendedDecision`.
- **Rule:** a first never auto-resolves. **Save:** `time` v1. **Tests:** granularity equivalence (§8), the interrupt and nomination caps.

**Activities and interaction structures** — Core · M2 model a system; M3 all four structures. Every doing: reservation, the structure engines, routine resolution and outcomes.
- **Owns:** `ActivityState`:
  - `Active?{Instance, ActivityId, Layer, StartedAt, ReservedMinutes, Scene?{Family, Case, Beat, UnitsLeft, Board}}`;
  - `Seen map<Family, set<Case>>`;
  - `Mastery map<Family, {Secure, Exposures}>`.
- **The engines** `Explain`, `Model`, `Fault` and `Coordinate` are pure `Step(def, case, board, action)` functions. Skill decides which methods exist and what they cost in units. A correct committed answer is never overturned by a hidden roll.
- **Commands:** `activity.begin|cancel`, `scene.act|commit`, `interact{slot, verb}`, `performance.begin|resolve`. **Events:** `ActivityStarted`, `SceneBeat`, `ActivityCompleted{outcome, learning, evidence, tags, relationshipDeltas}`, `PerformanceResolved`.
- **Save:** `activities` v1; a scene saves at its current beat. **Tests:** each engine against every authored case; `ResolveRoutine` equals foreground play under the same policy; at most two foreground classes a day.

**School and education** — Core · M2 day; M3 year; M5 Class XII, board examinations, university. Timetables, attendance, focus, assessments and clubs; later, enrolments.
- **Owns:** `SchoolState{Institution, Stream, Section, Timetable, Attendance map<Day, PeriodMask>, Focus map<Period, Id>, ForegroundUsedToday, Assessments[{Id, Groups[{Marks, Units}], Diagnostics[]}], Clubs, Enrolments[{Institution, Programme, Term, Status}]}`.
- **Commands:** `school.set_focus`, `.skip`; `assessment.survey`, `.order`. **Events:** `PeriodResolved`, `AssessmentMarked{diagnostics}`, `BriefPublished`, `ResultsPublished`.
- **Save:** `school` v1. **Tests:** a 90-minute paper is 18 units; the four diagnostic kinds stay distinct; skipping shows its consequence before the choice.

**Work and careers** — Core · M4 credentials; M5 careers; M8 all paths. Credentials, permissions, achievements, positions and commitments — three of the four records (methods live in Stats, relationships in Relationships).
- **Owns:** `CareerState`:
  - `Credentials map<Discipline, Grade>`;
  - `Permissions[{Id, Issuer, Level, Granted, Expires}]`;
  - `Achievements[{Id, Discipline, Tier, At}]`, which never decay;
  - `Positions[{Employer, Role, Grade, Since, ContractId}]`;
  - `Commitments[{Kind: term|contract|season|project, ExitAt, SunkCost, Transferable}]`.
- **Commands:** `career.apply|accept|resign|commit|request_trial`, `work.set_routine`. **Events:** `OfferMade`, `Promoted`, `PermissionGranted`, `CommitmentEnded`, `Dismissed`.
- **Save:** `careers` v1. **Tests:** hiring supplies labour, not credentials; no crown counts twice; management is never the compulsory final promotion.

**Money** — Core · M2 fares and pocket money; M5 full. Accounts, transactions and prices, in integer paise.
- **Owns:** `MoneyState{Accounts map<Id, Paise>, Recurring[{Payee, Amount, PeriodDays, NextDue}], Ledger ring<{Seq, At, Amount, From, To, Reason}>[400], MonthlySummaries}`. A price is the base price × the national price index × the district's Gate-risk modifier.
- **Commands:** `money.pay|transfer`. **Events:** `TransactionPosted`, `BalanceLow`, `PaymentMissed`.
- **Save:** `money` v1; the ledger folds into monthly summaries after 120 days. **Tests:** conservation — the sum of accounts plus external flows is zero; the fold round trip.

**Economy and ownership** — Core · M2 inventory; M5 contracts, debt, insurance; M8 enterprises. Everything owned or owed.
- **Owns:** `EconomyState`:
  - `Items[{Instance, Def, Owner, ConditionMilli, Where}]`;
  - `Property[{Kind: home|shop|land, Owner, Tenure, ValuePaise}]`;
  - `Contracts[{Kind: employment|rental|supply|service, Parties, Terms, Obligations}]`;
  - `Debts[{Creditor, Principal, RateBp, Schedule}]`;
  - `Policies[{Insurer, Cover, Premium, Claims[]}]`;
  - `SharedBudgets[{Members, Contributions, Rules}]`;
  - `Enterprises[{Stock, Prices, Employees[PersonId], Cash, Solvency}]`.
- **Commands:** `item.use|repair|buy|sell`, `contract.sign|end`, `loan.take|repay`, `policy.buy|claim`, `enterprise.found|hire|price|restock`.
- **Events:** `ItemWorn`, `ContractBreached`, `ClaimSettled`, `Insolvent`. Insolvency is an orderly state; the game never invents unlimited liability.
- **Save:** `economy` v1. **Tests:** wear stays bounded; insolvency ends in a defined state; claims reconcile against event damage.

**Household and housing** — Core · M2 one origin; M3 all six; M5 moving and cohabitation. Who the player lives with, where, who does what, and who cares for whom.
- **Owns:** `HouseholdState`:
  - `Current{Members[{Person, Role, Since, Until?}], Home{Location, Tenure, RentPaise}, BudgetRef, CareDuties[{Carer, Dependent, HoursPerWeek, Outcome}], Obligations[{Id, Outcome, Cadence, Assignee, Redesign}]}`;
  - `History[]`.
  - At rank 0, the household is the children's home and its care staff are members.
- **Commands:** `household.redesign|negotiate|leave|join|return_home`, `housing.move`. **Events:** `ObligationDue`, `ObligationCovered{by}`, `ObligationMissed`, `HouseholdChanged`.
- **Save:** `household` v1. **Tests:** obligations are outcomes, never animations; a redesign releases time permanently; every origin produces a valid opening day.

**Relationships** — Core · M2; M5 in full. Friendship, family, trust, estrangement, reconciliation and bereavement; conversation.
- **Owns:** `RelationshipState{Pairs map<(PersonId, PersonId), {Stage: acquaintance|friend|close|strained|estranged|reconciled|family|partner|memorial; Trust, Warmth, Respect, Owed: Milli; FirstActs bitset; Memories[≤32]{EventRef, At, Salience}}>}`. Pairs cover the player with anyone met, and NPC pairs among tier 0 only (at most 1,225). Adult romance depth is open (`GAME_PLAN.md` §12).
- **Commands:** `talk.open|choose|close` (conversation mode), `social.gift|ask|refuse|invite`. **Events:** `RelationshipChanged`, `MemoryRecorded`, `Bereaved`.
- **Save:** `relationships` v1. **Tests:** only the first meaningful act of a kind moves a relationship; a death turns the pair into `memorial` and never deletes it.

**Threads** — Core · M3. Persistent unresolved tensions that escalate on a clock: the story engine.
- **Owns:** `ThreadState{Active[≤40]{Instance, Template, Roles map<Role, PersonId>, Intensity, NextAt, Vars map<Id, int64>}, Resolved[{Template, Resolution, At}]}`.
- **Commands:** `thread.respond`. **Events:** `ThreadOpened`, `ThreadEscalated`, `ThreadExpressed{scene, channel}` (becomes a nomination or the week's one interrupt), `ThreadResolved`.
- **Save:** `threads` v1. **Tests:** every template resolves at maximum intensity; escalation respects the interrupt budget.

**Opportunity receipts** — Core · M2. What a commitment displaced — only alternatives the player knew about.
- **Owns:** `ReceiptState{Known[{Alternative, KnownAt, Source, Window}], Daily[≤120]{Lines[{Kind: did|displaced|covered|earned|spent|decided_by_order, TextKey, Args}]}, Weekly[]}`.
- **Events:** `ReceiptIssued`. **Save:** `receipts` v1; dailies fold into weeklies after 120 days.
- **Tests:** no line names an alternative whose `KnownAt` falls after the commitment.

**Aptitude Projection** — Core · M3. The school terminal: evidence per path with confidence, naming what it has not observed.
- **Owns:** `ProjectionState{Printouts[{At, Paths[{PathId, Band: strong|plausible|insufficient_observation, ConfidenceMilli, Observed[], Unobserved[]}]}]}`, produced by a pure `Project(stats, evidenceCoverage, content)`.
- **Commands:** `projection.queue`, an activity that costs a morning. **Events:** `ProjectionPrinted`.
- **Save:** `projection` v1. **Tests:**
  - a group activity narrows uncertainty without improving the outcome;
  - "unobserved" never displays as weak;
  - no raw number is ever shown.

**Life events and recovery** — Core · M5; M8 retirement. Disruptions with defined next actions, never a dead end.
- **Owns:** `LifeEventState{Situation{Kind: student|employed|unemployed|retraining|interrupted_education|insolvent|separated|bereaved|retired|recovering, Since}, Plan{NextActions[≥2], Supports[]}, History[]}`.
- **Commands:** `recovery.choose`. **Events:** `LifeEventStarted`, `RecoveryStep`, `SituationResolved`.
- **Save:** `life_events` v1. **Tests:** the validator proves at least two reachable next actions from every reachable state.

### 3.4 World

**Locations and interiors** — graph in Core, maps in UE · M1. Hubs, interiors, zones, interaction slots and travel.
- **Owns:** `LocationDef` content (see `SYSTEMS_DATA.md`); `LocationState{Closures[{Route, Until}], Locks, Eras}`; and `PlayerState{Location, Zone, TransformCm, YawCdeg, Camera}` (the transform is used for resume only).
- **Commands:** `travel.go{to, mode}`, `player.enter_zone`, `player.sync_placement` (on save and on map change). **Events:** `Arrived`, `RouteClosed`.
- **Placement:** `MASTER_PLAN.md` §4 streaming stands. Each district is one small hub map and interiors are level instances; the dungeon and tower are separate maps. Travel transitions join districts; there is no World Partition.
- **Save:** `locations` v1, `player` v1. **Tests:** the travel graph is connected; an editor audit checks that each slot has exactly one placed actor carrying its `SlotId`.

**Identity registry** — Core · M1 player and family; M6 in full. A person gets a persistent identity at first consequential reference, not at first physical meeting. A reference is being named in dialogue, news, rankings, correspondence, family history or any other player-visible record.
- **Owns:** `IdentityRegistry{map<PersonId, {Names (name-table indices), DateOfBirth, Nation, Pronouns, OriginKey, FirstRef{Kind, At, Ref}, Appearance, Facts[] (append-only)}>}`. A generated person's ID is `"p_" + hex16(Hash(WorldSeed, OriginKey))`; the authored cast use content IDs.
- **Events:** `PersonRegistered`. Registration happens as a side effect of publishing any visible record.
- **Save:** `identities` v1, at most 2 MB. **Tests:** identity and facts are immutable once written; the same OriginKey always yields the same person.

**NPC fidelity and crystallisation** — Core · M2 tier 0; M6 tiers 1–3. How finely a registered person is simulated — kept separate from who they are.
- **Owns:** `FidelityState`:
  - `Tier map<PersonId, 0|1|2>`;
  - `Tier0 map<PersonId, {Stats, Condition, Career, Finances, Goals, ScheduleId}>`;
  - `Tier1 map<PersonId, {Attr[6], PathStage, Employer, Location, Health}>`;
  - `Dormant map<PersonId, {Attr[6], Stage, LastContact}>`.
  - Cohorts and aggregates belong to Nations.
- **Crystallising** samples the cohort model with keyed RNG and backfills facts only from the event log before the reference instant.
- **Demotion** after 730 days without contact keeps identity and facts and summarises the rest. Re-promotion reproduces the same person, byte for byte.
- **Events:** `Crystallised`, `FidelityChanged`, `NpcLifeEvent{promoted|moved|married|emigrated|injured|died}`.
- **Save:** `npcs` v1. **Tests:** demote-then-promote equality; the tier-0 weekly update uses the player's own growth code, so rivals spend the same calendar.

**Schedules** — Core · M2. Declarative whereabouts resolved at event boundaries.
- **Owns:** `ScheduleDef` content, plus state `Overrides[{Person, Until, Block}]`.
  - `WhereIs(person, t)` is pure. Ordinary transitions schedule no events, so world state never depends on where the player stands.
  - Only transitions with an `onArrive` hook schedule an event.
- **Commands:** `npc.invite`. **Events:** `NpcArrived`, for hooked transitions only.
- **Save:** `schedules` v1. **Tests:** every schedule covers 24 hours on every day type; `WhereIs` gives the same answer live and after a year skip.

**Events and disasters** — Core · M3 authored incidents; M6 generated incidents and the twelve mechanisms. Hazard onset is fixed; consequence is earned.
- **Owns:** `WorldEventState`:
  - `Log[{Id, Kind, OnsetAt, Where, Severity}]`;
  - `Outcomes[{Event, Casualties, DamagePaise, ResponseMilli, Causes[{Kind, Ref, WeightMilli}], Affected[PersonId], PlayerContributionMilli, PlayerActions[]}]`;
  - `Effects[{Kind: route_closure|price_mod|school_closed|hospitalised, Target, Until}]`.
- **Onset cannot be prevented**, whether it comes from an authored date or a keyed seasonal draw.
- **Consequence** is `base × (1 − Σ mitigations)`. Each mitigation is aggregated from named contributors — institutions, NPCs and the player — so the player's share is exact and can be explained.
- **Events:** `HazardOnset`, `OutcomeResolved`, `EffectStarted|Ended`.
- **Save:** `events` v1; minor foreign incidents fold into aggregates. **Tests:** each authored event fires exactly once under any compression; contribution shares sum to the mitigation total.

**News** — Core · M3; M6 in full. The phone feed: rankings, incidents and the public record.
- **Owns:** `NewsState{Items[≤500]{At, Template, Args, Reach: local|national|world, SourceRef, Read}, Archive[{At, Template, Args}], MoodMilli}`.
- **Commands:** `news.read` (UI paused). Reading an item and looking away are both recorded. **Events:** `NewsPublished`, which registers every named person.
- **Save:** `news` v1. **Tests:** every template argument resolves.

**Changing world** — Core · M6 institutions and businesses; M8 decades. Replacement generations, evolving institutions, business turnover, technology adoption, changed interiors.
- **Owns:** `ChangeState{Institutions map<Id, {Kind, Founded, Status, ReputationMilli, Head, Capacity}>, Businesses per district [{Kind, Owner, Opened, Closed?}], Technologies map<Id, {Stage, AdoptionMilli per nation}>, Eras map<LocationId, EraId>}`.
- **The annual update:**
  - cohorts age one band and a new cohort of sixteen-year-olds enters;
  - institutions follow their funding and results;
  - businesses open and close by district economics (keyed RNG);
  - interiors change era while slot IDs persist.
- **Events:** `InstitutionChanged`, `BusinessOpened|Closed`, `TechnologyAdopted`, `EraChanged`.
- **Save:** `change` v1. **Tests:** after a 60-year soak, every slot still resolves.

### 3.5 Global

**Nations and Country Level** — Core · M3 static display; M6 India live; M7 all fifty. Continuous inputs, quarterly publication, annual review.
- **Owns:** `NationState{map<NationId, {...}>}`, keyed by the `GLOBAL_WORLD.md` §8 roster IDs (`nation_` plus the ISO 3166-1 alpha-3 code), where each nation holds:
  - `LevelMilli`;
  - `PathScores array<Milli,10>` — exactly 10% each; within a path, 70% senior/open and 30% junior/university;
  - `Modifiers{Containment24m, CasualtyRate, ResearchAdoption, Stability, Compact}`;
  - `Band` and `PriceIndexBp`;
  - `Cohorts[10 disciplines × 6 age bands]{Count, MeanMilli, SpreadMilli}`;
  - `Aggregates{Population, Employment, MigrationNet, Casualties, Sentiment}`;
  - `QuarterlyHistory` and `AnnualHistory`.
- **Events:** `CountryLevelPublished{old, new, bandChanged}` each quarter, and `AnnualReview`.
- **Save:** `nations` v1. **Tests:** no path exceeds its 10%; band effects apply exactly at a crossing; published history never changes.

**Rankings** — Core · M4 exam rank; M5 personal; M6 national; M7 world. Individuals, universities and nations.
- **Owns:** `RankingState`:
  - `Results[{Entrant, Discipline, Event, TierWeight, Place, Entries, FieldStrengthMilli, At, ScoreMilli}]` — tracked entrants only, 24-month window;
  - `Tables map<(Scope, Discipline, Division), {PublishedAt, Rows}>`;
  - `AllTime[]`;
  - `Universities map<Id, {Competitive, Verified, Development}>`.
- **Scoring:** `tierWeight × 100 × (1 − (r−1)/n)^1.5 × fieldStrength`, in fixed point (`x^1.5 = x × isqrt(x)`). The best four results count.
- **Events:** `RankingPublished`. Every name displayed, down to the table's publish depth, is registered.
- **Save:** `rankings` v1. **Tests:** fifty small events never beat four good ones; the per-institution cap holds; all-time records never decay.

**Competitions and the Convergence calendar** — Core · M5 school to university; M6 national; M7 continental and world. The yearly calendar:

| Months | Level |
|---|---|
| January–March | School, district, the national examination |
| April–June | University and open |
| July–September | National championships |
| October | Continental |
| November–December | The World Convergence |

- **Owns:** `CompetitionState{Instances[{Def, Year, Nation, Level, Discipline, Division, OpenAt, State, ResultsRef}], PlayerEntries[]}`.
  - An event the player does not attend resolves statistically; one they attend is played through the tower or the structures.
  - World finals have 80 entries: 50 guaranteed plus 30 merit, capped at 3 per nation.
- **Commands:** `competition.enter|withdraw`. **Events:** `CompetitionResolved`, `Qualified`, `SelectedForTeam`.
- **Save:** `competitions` v1. **Tests:** entry rules; result distributions over 10,000 seeds.

**Geopolitical tensions** — Core · M7. Five authored state machines, moved by standing.
- **Owns:** `TensionState{map<TensionId, {State, EnteredAt, PressureMilli, Contributions[{Person, WeightMilli, At}]}>}` for the Compact, the equipment monopoly, the awakening-age dispute, migration of the capable, and frontier Gates.
- **Commands:** `tension.contribute`, issued through career actions: a signature, a published story, a treaty clause. **Events:** `TensionStateChanged`.
- **Save:** `tensions` v1. **Tests:** transitions fire on their date or threshold whether or not the player acts; a contribution's weight scales with standing.

### 3.6 Supernatural

**Gates** — Core · M3 incidents and district risk; M6 live; M8 hunter breadth. Rifts ranked F–S; their risk shapes property, insurance and routes.
- **Owns:** `GateState{Gates map<Id, {Rank, Where, OpenedAt, State: open|contained|cleared|breached, Responders, OutcomeRef}>, DistrictRisk map<District, Milli>, Frontier[]}`. Onsets come from per-nation, per-season rank rates, keyed.
- **Commands:** `gate.join{gate, role}` (licensed roles only). **Events:** `GateOpened`, `GateBreached`, `GateCleared`, `DistrictRiskChanged`.
- **Save:** `gates` v1. **Tests:** onsets are identical under any compression; risk reaches prices only through Money.

**Tutorial dungeon** — rules in Core, instance map in UE · M4. Supervised F-rank; cannot be failed; reads conduct, not claims.
- **Owns:** `DungeonState{Run?{School, Party[PersonId], Room, Conduct[3]{Test, Observed[ActionTag]}, Done}}`, on the mission clock.
- **Commands:** `dungeon.enter|observe|assist|evade|improvise`. **Events:** `ConductRecorded`, `DungeonCompleted`.
- **Save:** `dungeon` v1, with a checkpoint at entry. **Tests:** every route ends awakened; no dialogue choice alone moves a conduct result.

**Tower** — rules in Core, maps in UE · M4 three floors; M7 competition floors. The virtual examination; defeat costs rank, not life.
- **Owns:** `TowerState{Run?{Context: exam|competition, Floor, Route, FloorResults[{Discipline, Depth, MethodTags, ScoreMilli}], Ejected}, Results[{Year, NationalRank, Credentials, EnvelopePaise, Licences[]}]}`.
  - Credentials are 80% discipline and 20% common judgment, with no combat in the common part.
  - Floor variation comes from the `tower` stream.
  - The examination can be retaken annually to age twenty.
- **Commands:** `tower.enter|choose_route|withdraw`. **Events:** `FloorCleared`, `Ejected`, `ExamResultsPublished`.
- **Save:** `tower` v1, with a checkpoint per floor. **Tests:** each of the three routes can be completed without combat; mixed routes share one support envelope.

**Combat** — rules in Core, execution in UE · M4. One tool grammar, evasion, one contextual action and one derived ability, built as a direct-control performance (§2.6).
- **Owns:** `EncounterDef` content, and portable `CombatRules` (damage, stamina, ability effects in `Milli`) that the UE runner calls on each hit. The results persist — injuries into Condition, scores into Tower — while encounter state is transient. GAS stays a spike.
- **Commands:** `performance.resolve`. **Events:** `EncounterResolved`.
- **Tests:** the rules; the envelope validator rejects impossible metrics; real Gates are never balanced like the tower.

### 3.7 Presentation (the `PerfectWorld` module; no persistent state except user settings)

| System | When | Responsibility and contract |
|---|---|---|
| **First/third-person camera** | M1 | **Modes:** `APWPlayerCameraManager` with FirstPerson (default), ThirdPerson, Orbit (creator) and Scripted. **First person:** the viewmodel is block arms visible only to the owner. **Comfort:** FOV 70–110 (default 90), head bob off by default, reduced motion. **Persistence:** the mode is saved in `player` for resume, and as a preference in user settings |
| **Player controller** | M1 | `APWPlayerController` and `APWCharacter` (CharacterMovement; Enhanced Input for keyboard, mouse and controller). Runs direct-control performances and submits their results; it only ever submits commands |
| **Interaction and look-at** | M1 | **Targeting** is measured from the character's eye point in both cameras, so first and third person share reach, targeting and validation. **Reach:** 250 cm; a 12° cone plus a 30 cm sphere sweep. **Priority:** overlapping candidates are ranked by profile priority, then angle, then distance. **Prompts** show the verb and its time cost. Conversation look-at works within 300 cm |
| **Phone interface** | M2 calendar, receipt; M3 news, messages, projection; M5 folio; M6 rankings | A flat, screen-aligned Slate overlay (not an in-world render target), fully navigable by keyboard and controller, with scalable text. Opening it enters `ui_paused` |
| **Block renderer and asset resolver** | M1; M9 swap | **Resolution:** `UPWAssetResolver` maps an asset ID to the soft path of its current representation per `ASSET_PIPELINE.md`, loading asynchronously at transitions. **Rendering:** HISM for repeated props; block characters rigidly skinned to the UE humanoid skeleton. **Coverage** report for M9 |
| **Audio** | M3 ambience; M4 tower | Sounds referenced by asset ID. Ambience follows location, hour and weather; barks follow events |
| **Front end** | **removed 2026-09-28; rebuilt in 3D (ROADMAP M1-23)** | **Adds:** title, Continue (the newest save across slots), Credits, and the creation stage with Identity, Stream and Review. **Routing:** loading follows the resume record. **Modes:** a front-end game mode runs `MAP_FrontEnd` only, and `APWGameplayGameMode` runs world maps. **Creator:** its subsystem becomes a façade over `UPWSimSubsystem` |

---
