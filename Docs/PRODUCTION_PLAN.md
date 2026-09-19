> Planning estimates only. The current authorized delivery is the starter package. Later phases are proposed work, not an automatic commitment. Latest user preference: one small, testable change at a time; GPT-5.6 Sol by default, Sonnet for design/review, Opus selectively.

# Perfect World: Awakening — Production Plan

## Premise & Constraints
Mobile-first life sim + Gate RPG (UE5.7, C++). Protagonist starts exactly 16, Class XI, fictional Nabapara/Nabapara Vidyalaya, West Bengal. 1000-point creator, 20 traits, nonlinear costs. Single-player, offline. Current state: starter code + docs only, no playable world. Windows is the dev host; Android real device is the mobile performance gate from early on. No delivery dates are promised below — ranges assume focused, uninterrupted work and will slip with scope creep, tooling issues, or team availability.

## Long Vision (context only, not committed scope)
Age 16 → school life → careers, expanding Bengal-wide, later India-wide, with Gates layered throughout. Phases 0–2 are near-term proposals; the small-step queue governs what is actually undertaken.

## Team Assumptions
- **Solo:** one experienced UE/C++ dev, all disciplines, ~15–25 hrs/week. Multiply ranges below by ~2.5–3×.
- **Small team (2–4, experienced):** roughly 1 gameplay/systems engineer, 1 UE/tech artist, 1 content/narrative+design, optional 1 generalist/QA. Ranges below assume this team.

---

## Phased Backlog

### Phase 0 — Foundation & Pipeline Proof (blocking everything else)
**Goal:** prove the toolchain end-to-end on target hardware before building content.
- Set up UE5.7 project structure, C++ module layout, source control with LFS.
- Character creator skeleton: 1000-point budget, 20 trait stubs, nonlinear cost curve (data-driven, not hardcoded).
- One tiny greybox scene (home interior) running and stable on a real low/mid Android device.
- Save/load skeleton (single save slot, offline).
- **Acceptance criteria:** project builds clean on Windows; APK deploys and runs on a real Android device at a defined min frame time; character creator allocates/spends points without exceeding budget or crashing; one save/load round-trip verified manually.
- **Estimate (small team):** 3–5 weeks. **Solo:** 8–14 weeks.

### Phase 1 — Core Loop Vertical Slice (Home → School → Para → First Gate)
**Goal:** the smallest playable loop across all four target areas, one in-game day only.
- Home: wake/sleep, one interactive routine (e.g., meal, study).
- School (Nabapara Vidyalaya): one class period, one NPC interaction, one simple choice with a stat/relationship effect.
- Para (neighborhood): free-roam greybox area, 1–2 NPCs, one social/errand interaction.
- First Gate: minimal combat/exploration loop (enter, one encounter, exit, reward), intentionally simple mechanics.
- Trait/point system feeding at least one visible gameplay effect (e.g., stamina, a dialogue option, or combat stat).
- **Acceptance criteria:** player can complete one full in-game day touching all four areas without a crash; each area has ≥1 functioning interaction; Gate encounter is winnable and losable; performance stays within the mobile frame budget on the reference Android device; no blocking bugs open.
- **Estimate (small team):** 6–10 weeks. **Solo:** 16–26 weeks.

### Phase 2 — 9-Day Story Slice
**Goal:** expand Phase 1 loop into a 9 in-game-day narrative arc, still tiny-core (home/school/para/first Gate only).
- Day-to-day progression state (calendar, relationship/stat persistence across days).
- Branching content for at least 2 of the 9 days based on earlier choices or trait build.
- Second Gate variant (new layout or encounter, reusing Phase 1 systems).
- Basic UI polish pass: creator screen, HUD, dialogue, day-transition.
- Audio pass: minimal ambient + UI SFX (placeholder-quality acceptable).
- **Acceptance criteria:** full 9-day arc playable start to finish with save/load surviving day transitions; at least one meaningfully different outcome from an early choice; sustained performance gate passes across a full playthrough on device; content review confirms no dead ends or softlocks.
- **Estimate (small team):** 8–14 weeks. **Solo:** 20–35 weeks.

Everything beyond Phase 2 (careers, Bengal-wide expansion, India-wide expansion) is explicitly out of scope for estimation until Phase 2 ships and is playtested.

---

## Scope-Cut Order (cut in this order under pressure, top first)
1. Second Gate variant (Phase 2) — ship with one Gate reused/reskinned instead.
2. Branching content beyond 1 day — collapse to linear with a single late-arc choice.
3. Para free-roam depth — reduce to a single fixed vignette scene.
4. School interaction variety — reduce to one canonical class + one NPC.
5. Audio pass — placeholder/no audio.
6. UI polish — functional-only, no visual pass.
7. Never cut: character creator budget integrity, save/load correctness, Android performance gate.

## Risk Register
| Risk | Impact | Mitigation |
|---|---|---|
| Android perf gate fails late (after content built) | High — costly rework | Test on real device every sprint, not just at milestones; set frame-time budget in Phase 0 |
| Nonlinear trait-cost math causes exploits/imbalance | Medium | Data-driven cost table, unit-test the allocator early |
| Scope creep toward long vision before slice ships | High | Freeze Phase 0–2 scope in writing; long vision items get a backlog, not sprint time |
| Solo/small-team bandwidth loss (illness, attrition) | High | Keep source control history clean enough that any team member can resume from last commit |
| Save data corruption | Medium | Versioned save format from day one; keep a manual save-file backup during testing |
| Art/content bottleneck (small team, no dedicated artist) | Medium | Greybox-first, placeholder art until loop is proven fun |

## Source Control & Backups
- Git with Git LFS for binary/UE assets; `.gitignore` tuned for UE5 (Intermediate, Saved, Binaries, DerivedDataCache).
- Commit at least daily during active work; tag builds that pass the device gate.
- Push to a remote (private repo) as the primary backup; do not rely on local-only history.
- Before any destructive git operation, check status and stash/commit first.

## Art Pipeline
- Greybox/blockout first for all environments (home, school, para, Gate) before any final art pass.
- Placeholder character meshes/animations during Phase 0–1; final creator-compatible assets only once trait system is stable.
- Mobile-first budget from the start: polycount/texture size caps set in Phase 0 based on the reference Android device, not adjusted downward later.

## Test & Device Gate
- Every merge to main: builds on Windows.
- Every sprint (minimum): deploy APK to a real Android reference device, verify frame-time budget and no crash across the current playable slice.
- Manual playthrough checklist per phase acceptance criteria above (no automated test claims made here since none have been run yet).
- Trait/point allocator gets targeted unit tests given its nonlinear-cost bug risk.

## Model Workflow (Claude usage)
- **GPT-5.6 Sol**, default for small routine implementation tasks; **Claude Sonnet**, complementary design/content and review: gameplay C++, UE Blueprint/C++ glue, content scripting, docs, routine debugging.
- **Opus**, selective use only: hardest architecture decisions (e.g., save-system versioning strategy, trait-cost formula design, cross-system Gate/RPG integration) — used sparingly to respect rate limits.
- Batch Opus-worthy questions rather than round-tripping; default back to GPT-5.6 Sol for small iteration and fixes, with Sonnet when appropriate.
- No claims here that any code has been inspected or tests executed — this is a planning document only.
