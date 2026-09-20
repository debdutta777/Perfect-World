# School, assessment and latent progression

**Document status:** core design, 2026-09-20. Refined with GPT-6 (`gpt-6-astra`) and reviewed by the owner. This is the authority on what attending class actually *is* as gameplay, how tests work, and how stats grow. Nothing here is implemented.

## The problem this document solves

The player will attend class dozens of times. If a class is a cutscene with an XP payout, it is a chore, and the game's central claim — that ordinary life builds the person — becomes a lie the player clicks through. Class must be worth playing the fortieth time or the timetable is decoration.

## Four interaction structures, not forty minigames

Every subject resolves through one of four structures. They share controls; they do not share meaning.

| Structure | What the player actually does | Subjects |
|---|---|---|
| **Construct an explanation** | Place a claim, its supporting evidence, and its limitation into three slots | Literature, history, economics, interpretation |
| **Model a system** | Connect quantities, then predict what a change does | Maths, physics, accounting, geography |
| **Investigate a fault** | Choose tests with different costs; eliminate causes | Computing, practical science, repair |
| **Coordinate a group** | Assign work, exchange information, settle a constraint, submit together | Projects, debate, events, practicals |

Four structures times many authored problem families gives depth without bespoke minigames. An essay's defensible interpretation must not be graded like a circuit with one hidden right answer.

### A worked example — the water tank

The class is modelling flow rates. A tank's level rose less than expected.

The player picks a goal: understand the model, test whether the measurements are trustworthy, or help the group explain it clearly. Then three beats:

1. **Build the model.** Connect remaining volume to net inflow. A practiced character already has the method; an unfamiliar one spends the beat asking for a worked example.
2. **Resolve the discrepancy.** Inspect measurement times, propose a leak, or compare the markings. Only one investigation fits before the group must report.
3. **Present a claim.** Give an estimate, a bounded range, or argue another measurement is needed.

**There is no universally correct posture.** Sometimes the measurements are fine and hedging wastes the period. Sometimes the "leak" is an inconsistent measurement interval. Sometimes the estimate is right and the group still cannot explain it.

### What varies on the fortieth class

What is unknown; which methods the character now has; what resources are scarce; the group's state; the player's role (solver, experimenter, recorder, critic, presenter); what today feeds into; and what failure means — a wrong model, an unsupported conclusion, wasted time, or an unclear explanation, never just "wrong answer".

Do not randomise these independently. Author compatible cases and track which the player has seen.

### Mastery must remove repetition

Once a problem family is secure, offer **Attend using established routine**: normal attendance, normal learning, no manual play. Manual play is for an unresolved question, a new technique, a social objective or a stretch problem — *never* a permanent XP bonus that makes automation feel like self-sabotage.

At most **two foreground classroom interactions per school day.** Other periods resolve through the timetable with a focus chosen in advance.

### Why attend rather than skip

School bundles scarce things efficiently: instruction, equipment, feedback, classmates, assessed evidence. Skipping stays legitimate — self-study can recover knowledge, a later appointment can recover feedback — but it cannot recreate a particular group experience. Consequences must be visible *before* the choice. **If skipping is never rational, the timetable is decorative.**

### What shipped games teach

- **Bully** gave classes tangible utility — chemistry supplied useful items. Borrow the utility; reject the bespoke-minigame-per-subject cost.
- **Persona 5** keeps classroom questions brief and tied to the calendar. Reject fixed factual questions: they become an answer-key lookup that tests the player's Googling.
- **Fire Emblem: Three Houses** connects instruction to future capability and allows automated routine study. Reject the motivation-maintenance treadmill.
- **Papers, Please** gives mundane work a real decision structure through evidence comparison. Borrow that for assessments; reject constant throughput pressure for ordinary periods.

## Tests and assessments

### The player-versus-character question, settled

**Character skill determines which methods are available and what they cost in time. Player judgment determines where and how to apply them.**

A player should be able to role-play a brilliant mathematician without being one. A mathematically able player should still gain something from understanding the problem. Both hold under this rule.

Two things are forbidden:
- Asking for a correct answer and then rejecting it because Intelligence is too low.
- Letting "use Intelligence" resolve a whole examination with no decision.

**Never invalidate a correct committed solution through a hidden stat roll.** Always state the cost before commitment.

### The academic loop

1. **A real brief beforehand** — topic coverage, permitted tools, question structure, marking criteria. Preparation means choosing what to understand, not guessing what the designer will spring.
2. **Revision consolidates methods, not answer cards** — "check units", "construct a counterexample", "separate evidence from inference". Having a note is not mastery.
3. **Survey the paper** — five question groups with marks, familiarity and estimated work. The character supplies the estimate, with honest uncertainty.
4. **Choose order and ambition** — secure familiar marks, gamble on a high-value unknown, or reserve time to verify.
5. **Execute through the same four classroom verbs.**
6. **Diagnostic marking** — a missing concept, a misapplied method, an unsupported claim and unfinished work are four different failures implying four different next actions.

**Time economy:** a 90-minute paper is eighteen five-minute units. Five groups worth twenty marks each. A practiced method clears a routine group in two units; deriving an unfamiliar one takes four; verification costs one. A prepared character clears and checks everything. An unprepared one must triage.

### Physical assessment

The same principle: the character's condition and training determine available effort and recovery; the player decides pacing, risk and when to spend themselves. Sports day, fitness assessment and PE class use effort allocation across events, reading opponents, and choosing whether to protect a result or chase a better one.

## Latent stat progression

### Every activity produces three separate records

The practical outcome; skill-specific learning; and *limited* evidence toward attribute growth. Relationship change and Tattva affinity are tracked separately. **Helping someone is not automatically Charisma XP.**

An activity may develop several skills, but it divides one finite learning allowance among them — otherwise the optimal action is always whichever has the most tags.

### The catalogue

Fifty-plus authored activities, all reusing the four structures, traversal or contextual dialogue. A representative sample:

| Activity | What the player does | Grows |
|---|---|---|
| Compare two reports of one incident | Identify shared facts, contradictions, missing evidence | Academics, Intelligence, Perception |
| Explain a concept to a younger pupil | Diagnose their *specific* misconception, choose another representation | Academics, Communication |
| Correct an old marked paper | Rebuild the faulty step, apply it to a changed problem | Subject mastery, Intelligence |
| Debug a school-club program | Reproduce a fault, inspect evidence, test a correction | Computing |
| Repair a bicycle puncture | Diagnose the leak, choose a patch, verify | Practical Repair, Perception |
| Reconcile a shop daybook | Match sales, stock, payments; explain a discrepancy | Commerce, Intelligence |
| Plan a Kolkata commute | Compare bus, Metro and walking against the authored timetable | Logistics, Perception |
| Negotiate a household study window | Explain a need, offer a workable trade, respect others' constraints | Communication, Charisma |
| Mediate a group-project dispute | Identify incompatible expectations, negotiate a specific agreement | Communication |
| Maintain a cricket scorebook | Track events accurately, find a useful pattern | Perception, Academics |
| Practise football passing under pressure | Read movement, select useful passes | Fitness, Agility, Perception |
| Resume an interrupted project | Reconstruct the next useful action instead of restarting | Relevant skill, Willpower |
| Ask for help early | Identify the actual blockage, choose the right person | Communication, self-management |

Festival activities must fit the actual date. **Religion, language, family background and stream must never carry innate stat multipliers.**

### Anti-grind: the rolling practice window

Within a narrow practice family, in a rolling 24 hours:

| Session | Learning credit |
|---|---|
| First | Full |
| Second | ~half |
| Third | ~one tenth |
| Further | Maintenance and output only — no new learning |

All variants in a family share the limit. **Changing running routes does not reset endurance practice.** A broader per-skill budget stops players cycling twenty barely-different families.

Show a plain forecast — *Useful practice* / *Consolidation* / *Little new learning*. Never make the player reverse-engineer the curve.

**Consistency uses a rolling window, not a streak.** Three useful sessions on separate days within seven consolidate a technique. Missing Tuesday does not erase Monday. Illness and rest must not trigger a punitive reset.

**Attribute growth is much slower than skill growth.** Repairing the same bicycle fifty times must not make a character universally brilliant.

### Why the obvious exploits fail

- **Running in circles:** movement accrues bounded daily physical load. Sustained identical effort stops producing adaptation. Holding sprint while stationary produces nothing.
- **Deliberate suffering:** hunger, sleep loss, injury and humiliation grant no resilience XP. Coping improves self-management; manufacturing the problem must never pay.
- **Generosity farming:** the first meaningful act can change a relationship; repeated gifts cannot. Charisma grows through handling communication challenges *including accepting refusal*, not buying approval.
- **Wealth:** better facilities reduce friction and improve practice conditions. They do not bypass adaptation limits. Shared school equipment is a viable baseline. Access still matters through time saved — removing that would contradict the game's thesis.

### Automation is legitimate

Once a routine is established, the player may reserve an honest block of game time for it and receive its normal bounded result. There is no design virtue in forcing a player to re-enact every productive hour.

### Awakening preserves all of this

Awakening adds new *applications* of existing capacities. It must not replace the life system with kill XP. Each Tattva must support several occupations: **Agni cannot mean "combat career", Jala cannot mean "compulsory healer."**

## Ten innovations, ranked by impact over cost

| # | Mechanic | Ratio |
|---|---|---|
| 1 | **Opportunity receipts.** After a commitment the phone shows what it displaced: "Repair done; ₹180 earned; library consultation missed." At the examination the player can trace gaps to real choices. Only known alternatives — never omniscient accusation. | 5.0 |
| 2 | **A projection that names missing evidence.** "Strong analysis; coordination under pressure unobserved." Doing a group activity narrows uncertainty even without improving the outcome. Lets players investigate who they are without chasing a hidden recipe. | 4.0 |
| 3 | **The same evening, unequal time.** At authored milestones, compare the player's evening with a classmate's: both intended two hours' revision; travel or care work cut one to forty minutes. A later intervention — shared notes, a changed rota, a room — changes that specific constraint. | 2.5 |
| 4 | **Mistakes travel.** A misconception left unresolved reappears as a weakness in an estimate, a repair, or a tower problem. Correcting it there updates the same mastery record. | 2.0 |
| 5 | **Abilities remember their origin.** A stabilisation ability learned through load testing reveals weak points; one from coordinated construction distributes strain across people. | 1.67 |
| 6 | **Notes need translation.** A brilliant classmate's compressed notes are initially unusable. A joint explanation converts them — and reveals a gap in the classmate's own understanding. | 1.5 |
| 7 | **Obligations can be redesigned.** A recurring household request has a required *outcome*, not a compulsory animation. Batch it, swap it, fix its source, or renegotiate timing — permanently releasing time while respecting someone else's workload. | 1.33 |
| 8 | **Parallel tower credentials.** A logistics challenge advances enterprise eligibility as far as a combat challenge advances hunter eligibility. Mixed routes give real hybrid qualifications, not an inferior miscellaneous score. | 1.25 |
| 9 | **Familiar problems, impossible rules.** The tower rebuilds a system the player knows — a water network, a school queue, a relay — and changes one physical rule. Prior knowledge gives a starting model; the player finds where it breaks. | 1.0 |
| 10 | **Rivals spend the same calendar.** Named classmates follow bounded schedules with different constraints and arrive with the preparation those schedules allow. Recruiting the strongest teammate costs them another opportunity. | 0.8 |

## The first prototype test

One classroom problem family, one academic assessment using its methods, one physical event, one non-combat tower application.

**Test the classroom through forty exposures with the abbreviation available.** If players engage only to collect progression, or cannot explain what changed their decisions, adding more subjects will multiply the problem rather than hide it.
