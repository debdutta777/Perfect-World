# The post-examination week

**Status: proposal pending owner acceptance (P5-01, 2026-11-22). Nothing here is implemented or verified as gameplay.** It freezes six first-week outlines so P5-02 to P5-06 have a fixed target. No grant, admission, licence, retake enrolment or money is awarded by anything below; those remain unimplemented, and offers are described as enquiries. **Owner decision 2026-11-22 (`GAME_PLAN.md` §10): one sitting per life. Entering, declining or missing the window uses it, Defer is allowed once, and only a student not yet awakened sits later. No week may offer a retake.** Authority: `GAME_PLAN.md` §1 (freedom acceptance rule, dignity), §4 Phase 6, §12 questions I to N; `AWAKENING_AND_EXAM.md` (Rewards, Failure); `PHASE5_PLAN_2026-11-22.md` §3.

## 1. Shared frame

- **The week** is day offsets 1 to 7 after the examination (Monday 2 February 2043): Tue 3 Feb is day 1, Sat 7 Feb is day 5, Sun is day 6, Mon 9 Feb is day 7. Offsets are the `dayOffset` values in `Data/Content/school_followup/`.
- **One shared school.** Every outcome returns to the same Nabapara Vidyalaya timetable (`activity.attend_class`, 45 minutes a period) and the same home or care-home routine. The six weeks differ in the consequential task, one conversation and its status shift, not in a separate game. Breadth is not the aim.
- **Invented people, existing names reused.** Amrita Basu (school adviser, staff room and library), Debashis Pal (Nabapara Lane repair workshop), Dr. Ananya Chatterjee (Community Clinic). No new people are needed.
- **Classification is the core's.** `SchoolFollowupOutcome` (`Source/PerfectWorldCore/Private/SchoolFollowupContent.cpp`) returns `strong` for result decile 1, `middle` for deciles 2 to 5, `poor` for deciles 6 to 10, plus `withdrawn`, `deferred`, `declined`. In this document top = `strong`, bottom = `poor`. The UI never recomputes this.
- **Routine, days two to seven, for every outcome** (reused, no new content): school days (Wed to Fri, Mon) attend lessons via `activity.attend_class` (45 min each); `activity.study_at_home` (120 min) at home or the care-home study room; `activity.household_chores` (60 min, `Data/Content/activity/`) or care-home rota duties; `activity.exercise_run`, `activity.eat_meal`, `activity.rest`, `activity.sleep_night`; the generic follow-up records `school_followup.p2.school` (day 1, 10:00, 30 min), `.study` (day 2, 18:00, 30 min) and `.rest` (day 1, 18:00, 30 min). Weekend is free time. Nothing in the routine requires supernatural practice or combat.
- **Rank-0 institutional care (Household Resources 0).** The week runs the same way. `place.home` is the care-home study room and common area (the UI already labels it "your home or care-home study room"); chores become the care-home rota; the staff member present replaces the family in each scene's tea and supper. No task requires a fee, a parent's signature, a phone or travel money: every destination is walkable from Nabapara Lane and every cost is Rs 0. The care-home variant is a walkthrough requirement of the P5-06 matrix, not a seventh path.
- **Zero supernatural practice.** No day requires practising, entering a Gate or using the awakened profile. A high result earned entirely by non-combat floors must still reach the strong week unchanged (P5-06 all-non-combat case). The awakening already happened; nothing after is compelled.
- **Executable today means**: P407U records are rendered by `Source/PerfectWorld/Private/PerfectWorldExaminationFollowup.cpp`, booked and attended through `Core/P/WorldExamFollowup.h` (`exam.followup.apply`). Only `place.school` appointments can be Attended; other destinations stay booked. Attending today charges the session's minutes once and records `p407u.followup.<id>.done.<instant>`. It teaches and awards nothing. So **every task below needs P5-02 to be "consequential"**; the table in section 4 says what else.
- **Missed-session recovery (all six).** Per `WorldExamFollowup.cpp`, an expired booking can be replaced and Cancel releases a reservation, with the next free slot searched over the following 32 days. The week's proposed rule: a missed session is never lost; it rebooks for the next free slot at the same time cost, with no penalty line and no change in how the adviser speaks. The player is told the next slot and what the missed hour displaced. P5-02 owns the rule; P5-03 owns per-sitting booking identity.
- **Refusal (all six).** Every task is optional and every offer carries "no obligation" wording. Refusing everything leaves the routine above, so the week is never empty. Cancelling and not booking are both valid, and neither is recorded as a failure.

## 2. The six outcomes at a glance

| Outcome | Selected record (P5-04/05) | Destination, actor | Slot | Time cost |
|---|---|---|---|---|
| Top | `school_followup.p2.p4_strong_mentor` | Nabapara Vidyalaya staff room, Amrita Basu | Wed (day 2) 16:15 | 20 min |
| Middle | `school_followup.p2.p4_middle_stream_review` | Nabapara Vidyalaya office, Amrita Basu | Fri (day 4) 16:00 | 25 min |
| Bottom | `school_followup.p2.p4_poor_workshop_apprenticeship` | Nabapara Lane repair workshop, Debashis Pal | Wed (day 2) 17:00 | 45 min |
| Withdrawn | `school_followup.p2.p4_withdrawn_school_mentor` | Nabapara Vidyalaya, Amrita Basu with a note from Dr. Ananya Chatterjee | Thu (day 3) 16:00 | 30 min |
| Deferred | `school_followup.p2.p4_deferred_community_volunteer` | Vidyalaya notice desk, then Dr. Ananya Chatterjee's Community Clinic | Fri (day 4) 16:30 | 30 min |
| Declined | `school_followup.p2.p4_declined_stream_civic_path` | Nabapara Vidyalaya, Amrita Basu | Mon (day 7) 16:15 | 30 min |

Slots are the records' authored values; P5-04/05 may move them only by editing those records. Walking time between school and workshop is not modelled; if P5-06 adds it, the Wed 17:00 workshop slot (school lessons end before 16:00) must disclose it as part of the cost.

## 3. The six weeks

For each: entry state, day one, routine, task, consequence, refusal, recovery. "Day one" is Tuesday 3 February.

### 3.1 Top (decile 1, `strong`)

- **Entry.** Completed the climb in the top decile, by any route; the result panel shows public rank, credentials and offers already. Nothing says the climb was combat; route-neutral wording is required.
- **Day one.** Back at school, the corridor is louder than usual and no one tells the player what to feel. Between lessons Amrita Basu leaves a note: invitations have arrived and can wait. The player sits the ordinary lessons; peers ask questions at the pace they choose (the "changed peer standing" is access, not a rank badge).
- **Days two to seven.** Routine as in section 1. Wed: the mentor session.
- **Task.** Mentor term plan with Amrita Basu: sort the invitations, name one thing to decline, name one to explore. Existing text (`phase2.a8.p4_strong_mentor.*`) says any invitation may be turned down and an unhurried year kept.
- **Consequence (proposed).** A recorded term plan listing the player's chosen items and the declined ones, visible on the next-steps panel; the player becomes eligible for one mentor-responsibility offer (helping a Class XI classmate with a revision table), itself optional. No admission or funding is awarded.
- **Executable today.** Attend at `place.school` exists (`PerfectWorldExaminationFollowup.cpp`, `WorldExamFollowup.h`), recording attendance only. Needs **P5-02** (task resolve and the term-plan record) and **P5-06** (panel). The mentor-responsibility offer needs a new P5-02 task entry, or is dropped.
- **Refusal.** Not booking, or Cancel. The adviser repeats the offer once, on day 7, as a note; no further pressure.
- **Recovery.** Rebook into the next free slot; the invitations remain listed.

### 3.2 Middle (deciles 2 to 5, `middle`)

- **Entry.** A solid result in the middle bands: credentials and some offers, nothing flashy.
- **Day one.** A lesson-day with a small difference: the stream question is now live. A classmate compares notes about Science, Commerce and Humanities. No one hurries the player.
- **Days two to seven.** Routine as in section 1. Wed afternoon, a short study block makes the choice more informed (optional).
- **Task.** Stream and subject review with Amrita Basu (`phase2.a8.p4_middle_stream_review.*`): compare subject options and, if wanted, record a tentative stream for next term. The text frames a change of direction as ordinary.
- **Consequence (proposed).** A recorded tentative stream and the one lesson it would add to the timetable; the player can see which of next term's periods change. A fee-waiver coaching enquiry (`phase2.a8.p4_middle_coaching_scholarship`, a spare record) may appear as an enquiry only.
- **Executable today.** Attend at school exists; the stream record and timetable preview need **P5-02** plus **P5-06**. The `middle_coaching_scholarship` enquiry is also school-destination and attendable, recording attendance only.
- **Refusal.** Skip the review; the stream stays as it was, and the choice is offered again before the term bell on day 7.
- **Recovery.** As section 1.

### 3.3 Bottom (deciles 6 to 10, `poor`)

- **Entry.** A low result, possibly no floor cleared. The panel has already shown the existing minimum of three actionable offers (`ROADMAP.md` M4-04). The player did nothing shameful; the written scene tone must say so by what the world does, not by reassurance.
- **Day one.** Tuesday morning is an ordinary lesson. At the end Debashis Pal, who repairs the school's old fans and projectors, has left the bench key and a hand-written slip at the office: "bring whatever's broken". Nobody at school mentions the rank. The player is free to go home instead; the week does not hinge on this.
- **Days two to seven.** Routine as in section 1. The `p2.study` and `p2.rest` records remain. `school_followup.p2.p4_poor_retake_prep` and `p2.retake` must not appear for a candidate who entered: there are no retakes (owner, 2026-11-22). P5-04 rewrites or retires them.
- **Task.** Bench visit at the repair workshop on Wed 17:00: watch Debashis Pal diagnose a small appliance, then do one defined repair step with the player's hands. The text states that a low tower rank has no bearing on bench craft (`phase2.a8.p4_poor_workshop_apprenticeship.*`).
- **Consequence (proposed).** A finished repaired object the player carries, on the home or care-home shelf (visible accomplishment), a recorded workshop reference from Debashis Pal and a bench-craft skill evidence entry through the existing learning rules. No wage and no apprenticeship place are awarded; talk of polytechnic bridges is an enquiry.
- **Executable today.** No. The workshop destination is a booking only (`PerfectWorldExaminationFollowup.cpp`: non-school destinations are not Attended). Needs **P5-02** (resolve the task through an existing activity or a new bench entry) and **P5-06** (bind to the workshop location). If P5-02 declines the work, the fallback is a school-hosted practical session authored in P5-04. It must not be a retake preparation, because the one sitting has been used.
- **Refusal.** Not going, or cancelling. The workshop stays open on later days at no cost; Debashis Pal never mentions the missed day.
- **Recovery.** The same slip invites any day the player drops in, via rebooking.
- **Dignity checks.** No combat; no comparison with a named classmate's rank; the first line the player reads is about the object on the bench, not the result. Owner judges this week's tone (question L).

### 3.4 Withdrawn

- **Entry.** The player left the tower early (safe-area withdrawal or in-climb withdrawal). Floor credentials earned before leaving remain and can be put to use.
- **Day one.** An evening at home, or with the staff and residents at the care home, over tea (`school_followup.p2.p4_withdrawn_home_rest`, `place.home`, day 1 at 17:30, 60 min, `kind` rest). No exam talk is required. It is a real scene with a person present, then the usual sleep.
- **Days two to seven.** Routine; the school return is gentler: lessons attended at the player's own pace, with no assignment attached.
- **Task.** Thursday meeting with Amrita Basu about pacing and next steps, with a visiting note from Dr. Ananya Chatterjee's clinic (`phase2.a8.p4_withdrawn_school_mentor.*`). Putting earlier floor credentials to work: choosing which credential to list on the next-steps page.
- **Consequence (proposed).** A pacing plan chosen by the player (for example, lighter evening study for two weeks) recorded as a visible schedule note, and a listed floor credential. No admission is awarded.
- **Executable today.** The rest scene is `place.home` and thus booked only; Attend exists only for the school meeting, recording attendance. Needs **P5-02** (plan record) and **P5-06** (home location presenter for the rest scene, or simply the existing `activity.rest`).
- **Refusal.** No meeting. The note stays at the school office for the player to read later.
- **Recovery.** As section 1.

### 3.5 Deferred

- **Entry.** The player used their one deferral. Their single sitting moves to the next 2 February and cannot be deferred again. Sitting it stays optional, and the player is not enrolled in anything.
- **Day one.** An ordinary school day; the twelve-month calendar idea (`school_followup.p2.p4_deferred_year_study_plan`, Wed 18:00, 45 min, `place.home`) is offered as a calm evening task, not a duty.
- **Days two to seven.** Routine as in section 1, with a useful current week: lessons, chores, a study block. The week must not be "waiting".
- **Task.** Friday enquiry at the Vidyalaya notice desk about helping Dr. Ananya Chatterjee's Community Clinic with afternoon patient-card sorting or the neighbourhood reading table (`phase2.a8.p4_deferred_community_volunteer.*`), hours agreed around classes and household routines.
- **Consequence (proposed).** A recorded volunteer slot, visible on the calendar (a standing, optional commitment the player can end), and a reading-table afternoon the following week. No pay.
- **Executable today.** The enquiry is school-destination and attendable (recording attendance only). The clinic visit itself has no place in the existing destination vocabulary (`place.school`, `place.home`, workshop): needs a **P5-06** clinic destination or a school-hosted card-sorting session via **P5-02**.
- **Refusal.** No enquiry. No commitment is created and no later sitting is booked automatically.
- **Recovery.** As section 1. Retake enrolment is never automatic (question K).

### 3.6 Declined

- **Entry.** The player declined the examination and may live a full life without it (`GAME_PLAN.md` §1).
- **Day one.** A normal school day; classmates talk about the tower and the player is not asked to explain. The adviser's note says that board subjects and civic work are an ordinary route.
- **Days two to seven.** Routine, with the usual lessons; on day 3 the existing declined record `school_followup.p2.p4_declined_vocational_craft` (workshop, Thu 17:00, 45 min) is a spare offer.
- **Task.** Monday meeting with Amrita Basu to map Class XII board subjects, direct degree admission at Bhagirathi State University or local civic work in Nabapara (`phase2.a8.p4_declined_stream_civic_path.*`). The text says a full adult life needs neither a tower rank nor supernatural practice.
- **Consequence (proposed).** A recorded route sheet naming board subjects and one civic activity (for example the neighbourhood reading table), and a timetable note. Direct admission is described as a pathway, never as an award.
- **Executable today.** School-destination and attendable (attendance only); route sheet needs **P5-02**, display needs **P5-06**.
- **Refusal.** No meeting. The route sheet is not created; ordinary school continues.
- **Recovery.** As section 1.

## 4. Implementation entries, by task

| Outcome | Exists today | Needed |
|---|---|---|
| Top | School Attend, attendance only | P5-02 resolve with term-plan record; optional mentor-responsibility entry; P5-06 panel |
| Middle | School Attend, attendance only | P5-02 stream record and timetable preview; P5-06 |
| Bottom | Booking only (workshop) | P5-02 task resolve; P5-06 workshop binding; fallback: a school-hosted practical session (P5-04), never retake preparation |
| Withdrawn | School Attend; home scene booking only | P5-02 pacing-plan record; P5-06 |
| Deferred | School Attend for the enquiry | P5-02 volunteer slot; P5-06 clinic destination or school-hosted session |
| Declined | School Attend, attendance only | P5-02 route-sheet record; P5-06 |

P5-04 writes text keys `post_exam_week.*` in `Data/Text/en/post_exam_week.json` for the six records; the existing `phase2.a8.*` text is reused unchanged until then. P5-03 must distinguish booking, attendance and task completion per sitting before any of these consequences become persistent.

## 5. Risks and decisions for the owner

1. **Bottom tone** (question L). The workshop week is the proposed answer to the M4-13 judgement. The owner must say whether bench craft with Debashis Pal reads as dignified agency or as a consolation prize.
2. **Workshop is the only non-executable task destination plus the clinic.** If the owner prefers a week fully playable at school in P5-06, host Bottom's practical session at school (P5-04) and swap Deferred to `p4_deferred_year_study_plan`; this halves binding work and loses place variety.
3. **Single-record weeks** may feel thin; the routine and spare records carry the rest. P5-10 judges this.
4. **Timing.** No selected slot falls on a weekend; the Declined task is on Monday of day 7, the last day of the week. Confirm that is acceptable, or move it by editing the record.
5. **Awards.** Every consequence here is a record, plan or skill evidence under existing learning rules. Any money, scholarship or admission needs a separate owner decision (question L).
6. **Sitting identity.** Booking flags have no sitting ordinal (P5-03); a second-year booking can collide with a first-week record.
