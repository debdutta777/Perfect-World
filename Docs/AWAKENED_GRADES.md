# Awakened grades and levelling

**Document status:** design, 2026-09-28, written on the owner's instruction to define "awakening grade levelling systems properly". It makes concrete what `AWAKENING_AND_EXAM.md` (the profile) and `PATHS_AND_PROGRESSION.md` (the ladders and "the power problem") already require. **Implemented as portable rules in `Core/AwakenedProgression` (ROADMAP M1-33).** Numbers are tuning data, not promises. Not yet reviewed by GPT-6 (Codex is not installed on the cloud PC).

---

## 1. What a grade is

Awakening grants a **profile**, never a class (`AWAKENING_AND_EXAM.md`). The profile has three parts:

1. **Tattva affinity:** a distribution across Prithvi, Jala, Agni, Vayu and Akasha summing to 100%. It changes *method*, never ceiling (`GAME_PLAN.md` §2).
2. **Six power dimensions**, trained separately (`PATHS_AND_PROGRESSION.md`, "the power problem" A): **Output, Precision, Range, Duration, Perception, Coordination.** Each is a rating from 0 to 1,000.
3. **The awakened grade**, from **F** (lowest) through **E, D, C, B, A** to **S**. A grade is a *certified* level of capability, awarded by an assessment, never by accumulated points alone.

**There is no universal level.** A grade never multiplies Intelligence, persuasion, craftsmanship, study or any ordinary skill. A B-grade awakened person with no medical training cannot operate. An F-grade surgeon remains a surgeon.

## 2. Levels inside a grade

Each grade is divided into **ten stages, 1 to 10**, shown as *F-3*, *C-10* and so on. The stage is simply how far the profile's rating has moved between the grade's entry threshold and the next grade's threshold. It is a readable progress marker, not a separate currency.

**Reaching stage 10 does not promote.** Promotion to the next grade requires passing that grade's **assessment** (§4). A person can sit at C-10 for years, because capability without assessed judgment is not certified.

## 3. How the dimensions grow

The dimensions grow only from **deliberate practice**, the same rolling practice window that governs latent stats (`SCHOOL_AND_PROGRESSION.md`):

- Practice is logged per dimension, in minutes, on a game day.
- **Diminishing returns within a day:** the first hour on a dimension is worth the most; after four hours further practice that day earns nothing.
- **A weekly ceiling per dimension:** gain over the last seven days is capped, so a month of steady practice beats a week of cramming.
- **Distance to the ceiling slows growth:** a dimension near its grade's upper band grows more slowly than one far below it.
- **Mastery removes repetition:** routine resolution (`GAME_PLAN.md` §5) applies practice at full value, so a player who has shown they can do something is never forced to replay it.
- **Money accelerates access, not adaptation** (`PATHS_AND_PROGRESSION.md`, H): better facilities raise the *quality* multiplier of a session up to a fixed cap. They never lift the daily or weekly ceilings.

The **rating** that sets the stage is the profile's four strongest dimensions averaged, so a specialist can advance without being good at everything, and a generalist is not punished.

## 4. Assessments and promotion

Each promotion is an assessment administered by the National Gate Authority (NGA) or an accredited academy:

| To grade | Rating needed | Every dimension at least | Also required |
|---|---|---|---|
| **E** | 150 | 60 | 20 hours of supervised practice since awakening |
| **D** | 300 | 120 | An independent field assessment in one operation type |
| **C** | 450 | 180 | Field assessments across three operation types; a withdrawal and a rescue demonstrated |
| **B** | 600 | 250 | Leading an audited operation; failure management assessed |
| **A** | 750 | 330 | Audited operations in three hazard families |
| **S** | 900 | 420 | International assessment of frontier capability, including failure management. **S is competence, not celebrity** |

Everyone awakens at **F-1** or higher: the profile's awakening reading sets the starting ratings, and the stage follows from them. The awakening cannot promote beyond F; every higher grade is earned.

A failed assessment costs time and a fee, never the grade already held. The earliest retake is after 30 game days.

## 5. Grade, licence and Gate rank

- **Grade** is capability. **Licence** is permission to operate commercially (hunter, clinician, engineer with Tattva certification). A licence can never exceed the holder's grade; many graded people hold no licence and never will.
- **Gate rank** (F to S) is a hazard classification. The NGA's recommended minimum is: a Gate of rank X is entered by teams led by grade X or higher, with members no more than one grade below. Entering beyond that is legal only in declared emergencies.
- **The Deep Realm** follows the same scale: danger rises with distance from a nation's Gate anchors and with depth (`WORLD_SCALE.md` §4, floors).

## 6. Decline and rest

Grades are certifications and **never decay**. Dimensions do, slowly: a dimension untrained for 90 game days loses 1% of its rating per 30 days after that, down to a floor of 80% of its peak. The grade stays; the stage shown can fall. Returning to practice recovers lost rating at double the normal rate until the peak is regained.

## 7. What this must never become

- A grind: the ceilings make an extra hour worthless after the first few.
- A class system: the grade is one axis of a life among many, and most of society's valued work is ungraded.
- A power ladder that buys the world: see `PATHS_AND_PROGRESSION.md`, "the power problem", A to H, all still binding.
