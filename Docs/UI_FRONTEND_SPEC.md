# Front-end specification: phases 2 and 3

**Design specification · 11 October 2026 · implementation and verification pending.** Unreal Engine 5.8, C++ Slate, voxel presentation. Based on both supplied critiques, screenshots, `Private/UI`, creation views/subsystem and design documents. Screenshots establish composition, not verified interaction.

Phase 1 owns interaction/scaling foundations, palette, Hind Siliguri typography, button/card treatments and title. Consume those components. Phase 2 delivers Traits, Household and Stream; phase 3 the remaining screens. Proposed structs are **core: needed** unless identified as existing. Settings/rendering additions belong to the engine adapter.

## 1. Shared contracts

### Layout, copy and fairness

At 1920×1080, creator bounds are 1728×972, centred with 96/54 margins; inner padding 32. Reserve 64 for step navigation and 72 for footer; headings and remaining content use flexible height. At 1280×720, bounds are 1232×688 with 24/16 margins and 16 padding; navigation/footer minimum heights are 48/56. Content scrolls; navigation, budget and footer stay visible. Below 1440 logical viewport width, use the narrow layouts below. Enlarged text may trigger the same reflow earlier. Never shrink a whole screen to fit. All specified heights are minimums; wrap text and expand rows.

Shared copy: “New life”; steps “Household”, “Traits”, “Identity”, “Stream”, “Review”; footer “Back to title” on Household, otherwise “Back”; “Next”, or “Start Life” on Review. Step accessible label: “Step {Number}: {Name}, {Status}”; statuses “Current”, “Complete”, “Needs attention”. Free navigation does not imply validity; only core validation enables starting. On returning from a Review Edit link, footer also offers “Return to review”.

Quoted strings are exact English source copy for LOCTEXT/string-table entries; braces are named `FText::Format` arguments. Authored values referenced by field are localized data, not omitted UI copy. Preserve user-entered text verbatim; never localize stable IDs or infer identity from names. Existing authored household, trait and stream names retain their canonical spelling. Numbers/dates use locale formatting. No new copy may imply innate advantages from religion, caste, language, appearance, stream or family structure. Resources, support and housing security remain separate. Awakening is compulsory; supernatural practice afterward is optional. No career is a lesser path.

### State and input contract: applies to every screen

| State | Required behavior |
|---|---|
| Default | Readable phase-1 surface and text tokens; passive text never resembles a button. |
| Hover | Phase-1 hover treatment; never changes data or selection. |
| Focus | Independent visible focus outline, immediately rendered; scroll fully into view. |
| Selected | Check and selected marker plus accessible “Selected”; remains visible while focus moves. |
| Disabled | Reason visible beside the action; removed from action traversal, retained in accessible reading order. |
| Error | Icon, plain explanation and relevant action; never color alone. Preserve entered data and focus. |

Tab/Shift+Tab traverse controls in documented order; F6/Shift+F6 move between regions. Controller D-pad navigates, left stick mirrors it, A activates, B backs out; LB/RB traverse creator steps, except inside modals or text editing. Arrows operate lists/selectors; do not also navigate screens. Text fields retain editing keys and IME handling. Modal focus is trapped; Escape/B cancels, default focus is the safe action, closing restores its invoker. Consume activation release before exposing underlying controls. No destructive global shortcut. Every shortcut also has a visible control; footer hints use device glyphs with “Select”, “Back”, “Change rank”, “Apply rank” as applicable.

Use phase-1 hover/selection motion. Content crossfades: 160 ms; drawers: 180 ms; reduced motion: instantaneous, stationary previews/camera. Numbers update immediately. Discard stale focus/preview revisions. Use accessible controls and scroll regions; preserve active widgets during edits.

**Shared acceptance:** run every screen at both target sizes and 150% text; keyboard-only and controller-only, long translated strings and Bengali input. Nothing essential clips, falls behind the footer, depends on hover, or loses focus after resizing. Disabled reasons remain readable. Unknown adapter failures say “We couldn't complete that change. Please try again.” without exposing filesystem paths.

## 2. Phase 2 — Traits

### Purpose and composition

Allocate exactly 1,000 points while understanding each change. Heading “What is your background?”; introduction “Spend exactly 1,000 points across twenty traits. You can change any choice before starting.”

At 1080p, body columns after two 24-unit gutters: category navigator 18%, trait list 47%, inspector 35%. Rows minimum 76 high. Budget strip, minimum 56 high, sits above footer; toolbar above columns contains “Choose a starting background”, allocation name, “Undo”, “Redo”, “More”. At 720p, category becomes a full-width selector; list fills width and the selected row's inspector expands directly below it. Only one inspector exists. Its effect columns stack “Current” then “Proposed”; opening it scrolls its heading into view. Budget wraps to two lines; toolbar wraps without hiding controls.

Navigator labels in canonical order: “Appearance & presentation”, “Family, resources & support”, “Physical”, “Cognition”, “Talents”, “Social”, “Prior experiences”. Each adds “{Points} points”. Selecting a group filters rows, retaining all seven totals and the overall budget. Initial group is the last visited, otherwise Appearance. Each row shows canonical name, description below, five labeled positions “0”, “1”, “2”, “3”, “4”, and “{Cost} points allocated”. One composite selector is a Tab stop; all five positions remain accessible and clickable.

### Preview, inspector and budget

Focus selects a trait for inspection, not a different rank. Left/Right or clicking a position proposes a rank without mutating the ledger. Current rank has a check; proposal has an outlined marker labeled “Proposed”. Enter/A on the selector, or inspector “Apply rank”, commits once. Escape/B first cancels a pending proposal; a second backs out. Moving to another row/group discards the uncommitted proposal. Pointer hover never changes it.

Inspector copy: trait name; “Current → Proposed”; “Rank {Rank}: {Meaning}”; core effect lines; “Allocated: {CurrentCost} → {ProposedCost} points”; exactly one of “Costs {Points} points”, “Returns {Points} points”, “No change”; “Apply rank”; “Show all ranks” / “Hide all ranks”. Same-rank proposal disables Apply. Invalid proposals remain inspectable, with Apply disabled and core message inline both below selector and in inspector. Never silently lower another trait. The full ladder shows all five meanings, costs and effects, without five simultaneous edit controls.

Sticky strip: “{Remaining} points remaining”; “{Spent} / {Budget} points allocated”; “Rank 4: {Used} / {Maximum}”; four numbered slot marks with accessible “Rank 4 slot {Number}: {State}”, where state is “Used” or “Available”. A thin seven-segment allocation bar is optional decoration backed by the visible navigator totals. Empty budget is neutral; valid exact allocation gets check plus “All points allocated”. Incomplete allocation never blocks exploring another step.

Core message catalog: “This change needs {Needed} points; {Remaining} remain.”; “You can have at most {Maximum} traits at rank 4. Lower another rank-4 trait first.”; “Allocate the remaining {Remaining} points before starting.”; “This life has already started. Its starting background is fixed.” Unexpected invalid rank/ID: “This trait couldn't be changed. Please reopen the screen.” Bind message codes/arguments, not English string matching.

### Rank content and data

Existing `FPWTraitView.Id/DisplayName/Group/Rank/Cost` supplies rows; `FPWCreationSummary.TotalBudget/Spent/Remaining/RankFourCount/MaxRankFour` supplies budget. `SetTraitRank` remains the commit path. Replace numeric prose in `PWTraitText.cpp` with core-authored presentation records.

**core: needed** `FPWTraitDefinitionView { Id, ShortDescription, Ranks[] }`; each `FPWTraitRankView { Rank, CumulativeCost, Meaning, EffectLines[], AvailabilityNote }`. `FPWTraitChangePreview { DraftRevision, TraitId, CurrentRank, ProposedRank, CurrentCost, ProposedCost, Charge, Refund, bCanApply, MessageCode, MessageArgs, ResolvedHome }`; `FPWTraitGroupView { Id, DisplayName, AllocatedPoints }`. Core calculates totals, eligibility, differences and effects. UI only formats values. Availability notes disclose effects not yet active; never advertise deferred Creative skill as currently functioning.

Every definition has five explicit rank records. Meanings for nineteen traits: R0 “Ordinary baseline”; R1 “Noticeable tendency”; R2 “Practised advantage”; R3 “Strong part of your background”; R4 “Exceptional starting circumstance”. Costs are 0/25/60/110/180, supplied by core. Resources meanings instead: “Children's-home upbringing”, “Modest household resources”, “Steady household resources”, “Comfortable household resources”, “Ample household resources”. Ordinary R0 effect: “No added starting effect. This does not prevent learning through play.” Resources R0: “You grew up in a children's home, with a staff caregiver. No family household reserve.”

The table supplies exact short descriptions and effect templates. Core materializes each rank's effect, using 1/2/3/4 for `{Percent}` and 10/25/45/70 for `{XP}` at R1–R4. These are authoring values, never Slate formulas. Display names and stable IDs remain those in `CHARACTER_CREATION.md`.

| Trait | Short description | R1–R4 effect copy |
|---|---|---|
| Physical Presence | “A learned bearing that steadies first impressions.” | “First-impression composure window: +{Percent}%.” |
| Personal Presentation | “Preparation and care in how you present yourself.” | “Stress from formal school and social checks: {Percent}% lower.” |
| Household Resources | “The resources available where you grew up.” | “Household reserve: {Reserve}. It supports household needs; it is not cash you carry.” |
| Guardian Support | “Care and backing from the adults around you.” | “Stress recovery from supportive home interactions: +{Percent}%.” |
| Housing Stability | “A settled place to rest.” | “Sleep recovery at your starting home: +{Percent}%.” |
| Vitality | “Your starting physical health.” | “Starting maximum Health: +{Percent}%.” |
| Strength | “Practice with physical effort.” | “Carry capacity and force-task tolerance: +{Percent}%.” |
| Agility | “Moving with confidence on your feet.” | “Starting maximum exertion stamina: +{Percent}%.” |
| Analytical Reasoning | “Working carefully through problems.” | “Progress from logic and problem-solving activities: +{Percent}%.” |
| Learning Discipline | “Staying with a study session.” | “Focused-study efficiency before fatigue: +{Percent}%.” |
| Perception | “Taking time to notice details.” | “Observation prompt timing window: +{Percent}%.” |
| Technical Aptitude | “Experience with machines, repairs and code.” | “Starting Computing XP: +{XP}.” / “Starting Practical Repair XP: +{XP}.” |
| Creative Aptitude | “Making things and expressing ideas.” | “Starting Communication XP: +{XP}.” / “Creative skill allocation: +{XP} XP. This skill is not active in this build.” |
| Athletic Aptitude | “Experience with sport and physical training.” | “Starting Fitness XP: +{XP}.” |
| Empathy | “Paying attention to how people feel.” | “Emotion-reading prompt timing window: +{Percent}%.” |
| Confidence | “Practice speaking up.” | “Stress from public speaking and assertion: {Percent}% lower.” |
| Community Connections | “Introductions around the neighbourhood.” | “Neighbourhood contact leads: {Count}. An introduction is not an automatic friendship.” |
| Academic Foundation | “Schoolwork you have already put in.” | “Starting Academics XP: +{XP}.” |
| Practical Experience | “Hands-on practice with everyday repairs.” | “Starting Practical Repair XP: +{XP}.” / “Starting household-task familiarity: +{XP}.” |
| Adversity Resilience | “Recovering with rest and support.” | “Recovery from noncombat setbacks after rest or support: +{Percent}%.” |

Reserve values: ₹500/₹1,000/₹2,000/₹4,000; contact counts: 1/2/3/4. Inspector footer: “These are starting circumstances. They do not choose your career or supernatural ability.” Resources additionally shows the preview's resolved home; no warning styling for care.

### Presets, history and acceptance

Drawer: modal right-side 55% width at 1080p, full content width at 720p; scroll body, fixed actions. Title “Choose a starting background”; explanation “Start with an allocation, then make it your own.” Five entries: “Balanced Scholar”, “Resourceful Maker”, “Athletic Organizer”, “Creative Connector”, “Independent Generalist”. Select to preview; never apply on selection. Detail headings: “Strongest backgrounds”, “Changes from your allocation”, “Your opening home”; difference line “{Trait}: rank {Before} → {After}”; “No allocation changes” when identical. List every changed rank, including zeros; show core-ranked top three strengths, ties in catalogue order, and preview budget/rank-4 totals.

Athletic Organizer and Independent Generalist explicitly show “This background starts with Household Resources at rank 0: you grew up in a children's home.” Actions: “Cancel”, “Apply background”. Successful application closes drawer and announces “{Background} applied. You can undo this change.” Allocation name is matched `FPWPresetView.DisplayName`, otherwise “Custom”.

More menu: “Clear allocation”, “Reset to Balanced Scholar”. Both open the same preview, with “Apply changes”; Clear preview says “All traits will be rank 0. Your opening home will be a children's home. Allocate 1,000 points before starting.” Reset changes only ranks. Neither calls whole-creator reset. **core: needed** `FPWAllocationPreview { DraftRevision, PresetId, Changes[], StrongestTraits[], Summary, OpeningHome }`, atomic Apply/Clear/Reset, and `FPWAllocationHistory { bCanUndo, bCanRedo }`. History snapshots ranks only; restored home resolves against current household. New allocation edit clears redo; history survives step navigation, ends on load/new life/start. Undo/Redo are one transaction per operation.

Focus regions: toolbar → navigator → list → inspector → footer; narrow inspector follows its row. Up/Down changes rows, Left/Right proposes; F6 reaches inspector. Drawer order: preset list → preview scroll → Cancel → Apply. Ctrl+Z/Ctrl+Y invoke history outside text fields; controller uses visible buttons.

**Acceptance:** preview R2→R3 costs 50; R3→R1 refunds 85; cancel changes nothing. Fifth R4 refuses before mutation. All seven group totals sum to spent. Apply/undo each preset restores identical ranks and resolved home; repeated undo/save/reload creates no points. Test Clear as one transaction. All twenty definitions expose five rank records. At 720p selection never hides the budget or Apply action.

## 3. Phase 2 — Household

Purpose: choose living arrangements and understand the actual opening home. Heading “Where did you grow up?”; introduction “Choose your starting household. This choice costs no points. Household Resources can change your opening home.” Body is 35% list / 65% details, 24 gap at 1080p. At 720p, use five full-width compact rows followed by detail in one scroll region; preserve selection when scrolling.

List uses `GetHouseholds()` and `FPWHouseholdView.Id/DisplayName/GuardianExamples/bSelected`. Exact pairs: “Parent household” / “One or two parents or stepparents”; “Extended family” / “An aunt, uncle or multigenerational adults”; “Grandparent household” / “One or two grandparents”; “Adult sibling household” / “A sibling aged 21 or older”; “Appointed guardian” / “A family friend or hostel warden”. Activate calls `SetHouseholdTemplate`; focus alone previews nothing.

Detail headings “Your opening home”, “Everyday life”, “Care and support”. `GetOpeningHome()` supplies `HomeKitName`, `DressingTierName`, `DressingDescription`, `HomeKitId`, `DressingTierId`, `ResourceRank`, `bGrewUpInCare`; display “{HomeKitName} · {DressingTierName}” and description. A cached voxel vignette may occupy top 35% of details; omit its space if unavailable. Never substitute a photograph or fabricate a built room. Accessible description is the same home text.

Everyday-life copy, **core: needed** `FPWHouseholdView.EverydayText`: parent “Work schedules and school plans share the same calendar.”; extended “Shared rooms mean making space for one another's plans.”; grandparent “Everyday care can flow in both directions.”; sibling “You and your adult sibling work out the routines of a shared home.”; guardian “You and your guardian agree on routines and responsibilities.” Common care copy: “Resources, support and housing security are separate choices. Every starting home has a place to sleep, meals, storage and access to school.”

At rank 0, detail title becomes “Growing up in a children's home”; caregiver “A staff caregiver”; everyday text “School mornings begin alongside other residents, with staff helping the day get started.” Neutral explanatory text: “Household Resources is rank 0. Your opening home is a children's home. Your household choice is remembered for ranks 1–4.” List caption “Remembered household”; rows remain selectable, with accessible “Remembered for ranks 1–4”. Add “Edit Household Resources” linking directly to that trait. Never show the template's room as the resolved room. `FPWCreationSummary.CareNote/GuardianText` must adopt this copy through the adapter.

States follow §1; initial selection is core `bSelected`. Failures remain beside the list with core message; the previous home stays visible. Focus: five rows → resource Edit link → footer; F6 reaches scrollable details. Up/Down moves focus, A/Enter selects. Details crossfade only; no automatic camera orbit.

**Acceptance:** test all five templates at Resources 0 and 1–4. Raising Resources restores remembered selection; Guardian Support does not alter resources. Care has equal text size, contrast, preview area and navigation. No family affection or deprivation is inferred from template or rank.

## 4. Phase 2 — Stream

Purpose: choose a school timetable with open later routes. Heading “Which stream will you study?”; copy “A stream sets a timetable, not a destiny.” Three equally sized cards occupy the top 30% of body at 1080p; below, 60% sample day / 40% route information and optional reason. At 720p use three equal stacked compact cards, then sample day, routes, reason in one scroll region. No recommended badge or prestige ordering.

`FPWStreamView.Id/DisplayName/TimetableSummary/bSelected` and `SetSchoolStream` supply selection. Exact card text: “Science” / “Explore physical systems through models and practical investigation.”; “Commerce” / “Explore accounts, exchange and the decisions behind them.”; “Humanities” / “Explore people, places and ideas through evidence and interpretation.” Author these summaries into the data.

Selected detail “A sample school day”; caption “An illustration of school life at Nabapara Vidyalaya, not a fixed schedule.” Common rows: “Assembly — Meet your class and hear the day's notices.”; “Class time — {StreamActivity}”; “Shared work — Prepare an explanation with classmates.”; “After school — Make time for a club, a household task or rest.” Science activity “Model water flow and investigate a measurement.”; Commerce “Reconcile a daybook and explain a discrepancy.”; Humanities “Compare two accounts and build a supported interpretation.” No invented bell times. Caption “At most two classes a day need foreground play. Other periods follow your timetable.”

Routes heading “Room to change direction”; copy “Every stream starts with the same Academics XP. Trait XP is added separately. Later study and prerequisite courses can open other routes, including engineering from Humanities.” Reason label “Your reason (optional)”; hint “What draws you to this stream?”; helper “This is your note. It does not change your stats.” Empty is valid; retain note when switching.

**core: needed** extend `FPWStreamView` with `SampleDay[] { Label, Description }`, `RouteFlexibilityText`, `EqualStartingXPText`; identity validation metadata supplies reason limit, currently 60 in Slate. `GetSchoolReason()` supplies text. Commit on text completion, preserving IME. Stream refusal shows core message below cards; invalid reason remains editable inline. Default selection comes from core. Focus: cards → sample-day scroll → reason → footer; Left/Right moves cards at wide size, Up/Down at narrow; activate selects. Content crossfade follows §1.

**Acceptance:** three selections have identical size, cost and starting Academics XP; background ranks remain unchanged. Empty reason works. Sample day changes without focus movement; no route promise becomes an automatic qualification.

## 5. Phase 3 — Identity

Purpose: freely define the person. Heading “Who are you?”; copy “Identity and appearance cost no points and do not change your stats.” Wide body: form 65%, static voxel character preview 35%; narrow: full-width form with preview below, collapsed initially behind “Show character” / “Hide character”. Preview uses existing `GetAppearance()` record; no new appearance editor is introduced here.

Form order/copy: “Given name”, “Family name”, “Your name: {FullName}”, “Date of birth”, sublabels “Day”, “Month”, “Year”, “{Age} at story start”, “Choose a date from {Earliest} to {Latest}.”, “Pronouns”, “Primary language”, “Home city: Kolkata”. Name hints: “Enter your given name”, “Enter your family name”; helper “Bengali, English and mixed-script names are welcome.” Never transliterate automatically or infer caste/religion. Pronoun choices “she/her”, “he/him”, “they/them”, “Use my own”; custom hint “Enter your pronouns”. Language choices “Bengali”, “Hindi”, “English”, “Urdu”, “Nepali”, “Use my own”; hint “Enter your primary language”. Helper “This does not change the interface language.”

`FPWIdentityView` supplies all named values, `BirthDay/Month/Year`, `AgeOnCampaignStart`, `EarliestDay/Month/Year`, `LatestDay/Month/Year`. Use existing individual setters. Date control is three editable combo fields, with valid dropdown choices and whole-date validation on commit; invalid typed values remain visible until corrected. Never silently clamp. Current inclusive window is 3 June 2025–2 June 2026. Months use localized full month names. Invalid date copy: “Choose a valid date between {Earliest} and {Latest}. You must be 16 when the story begins.”

**core: needed** `FPWIdentityValidation { Fields[] { FieldId, bRequired, MaxGraphemes, MessageCode, MessageArgs }, ValidBirthDates }`. Existing Slate limits 40/40/16/16 are not authoritative Unicode validation. Core must publish and enforce limits; preserve conjuncts and IME composition, validate on commit, and keep invalid local text across navigation. Errors: “Enter {Field}.” or “Keep {Field} within {Limit} characters.” Start is blocked while local edits are uncommitted/invalid. Do not overwrite them during model refresh.

Default values come from draft. Focus follows form order, then preview toggle/footer; combo arrows navigate, Enter/A accepts; controller A opens text entry, B closes it before page navigation. Selected chips follow stored/custom value; unavailable preview toggle is omitted. No avatar rotation without direct input. Errors below fields, §1 states elsewhere.

**Acceptance:** enter “শ্রেয়া” and mixed-script names, paste long conjuncts, cancel composition, switch steps and return. Invalid 31 April and both outside-window dates reject; both boundaries accept. Changing pronouns/language never changes rank, XP or preview body.

## 6. Phase 3 — Review

Purpose: picture the opening and make a deliberate saved commitment. Heading “Your first morning”; wide 60% morning/summary, 40% backgrounds; narrow stacks both in one scroll region. Morning copy “Kolkata · 2 June 2042 · 06:00”; “You wake at {HomeName}. Class XI at Nabapara Vidyalaya begins today.”; “There is time to eat, help with a task or leave early. Your choices will shape the morning.” Caption “A preview of your starting circumstances.” Care uses resolved home, not remembered family.

Summary labels “Name”, “Date of birth”, “Pronouns”, “Primary language”, “Home”, “Caregiver”, “School stream”, “Your reason”; omit empty reason. Use corresponding `FPWCreationSummary.FullName/BirthDateText/PronounsText/LanguageText/OpeningHomeText/GuardianText/StreamName/StreamReason`, with age and city. Links “Edit identity”, “Edit household”, “Edit stream”, “Edit traits” target section and restore Review return path. Background heading “Your starting backgrounds”; show highest three ranks with catalogue-order ties; “Show all twenty traits” / “Show fewer traits”. Expanded list includes rank 0, cost and per-rank effect. Existing `Summary.Traits` excludes zeros: **core: needed** `AllTraits[]` and `StartingHighlights[]`, plus `FirstMorning { HomeName, DateText, TimeText, Description }`.

Budget uses §2 copy. `bReadyToStart`, `BlockingMessage`, `Warnings` remain authoritative; **core: needed** `ValidationIssues[] { FieldId, MessageCode, MessageArgs, TargetStep }` for inline issues beside Edit links. Care information is neutral, not a warning. Footer Start disabled with specific reason until both core and local fields validate.

Start opens slot selection, then a true modal (640 maximum width; viewport minus 32 at 720p). Copy “Begin this life?”; “Save destination: Slot {Slot}”; “Your starting choices will be fixed. You can continue this life from this slot.” Actions “Not yet”, “Begin this life”; focus Not yet. Occupied destination additionally names “Replace {Name} in Slot {Slot}? Its saved progress will be replaced.” No save replacement on merely selecting a slot. All occupied: “Choose a slot to replace, or go back to keep editing.” Revalidate slot revision before commit. Failure leaves draft/review intact: “Your life couldn't be saved. Try again or choose another slot.” Actions “Try again”, “Choose another slot”, “Keep editing”.

**core: needed** transactional `PrepareStart { DraftRevision, SlotIndex, SlotRevision, bWillReplace }` and commit result; start/save must succeed together before entering world. Existing `StartLife()` and `SaveToSlot()` alone do not prove this guarantee. Focus: Edit links → traits disclosure/list → footer; modal slot rows → Cancel → confirmation. No auto-start timer; §1 motion.

**Acceptance:** all twenty traits visible on expansion; Edit returns correctly. Test occupied slot, changed slot revision and write failure; none loses draft or enters an unsaved life. Cancel never overwrites. Confirmation names the actual destination.

## 7. Phase 3 — Pause and Load

### Pause

Purpose: stop play and manage continuation. Wide panel 480 units at left with 64 margin; narrow 420 with 24 margin, vertically scrollable, over frozen world. Heading “Paused”; metadata “{Name}”, “{Date} · {Location}”, “Last saved: {Time}” or “Not saved yet”. Ordered controls “Resume”, “Save”, “Settings”, then separated “Return to title”, “Exit game”. Save status “Saving…” / “Saved to Slot {Slot}.”; disable duplicate Save while pending, keep Resume available unless transaction requires a short lock.

Existing HUD `ResumeGame/SaveActiveGame/ReturnToTitle/QuitGame` actions remain. **core: needed** session view `FPWSessionView { FullName, DateText, LocationText, ActiveSlot, LastSuccessfulSaveTime, bHasUnsavedChanges, bSavePending }`. Dirty exit modal “Leave this life?”; “Changes since your last save will be lost.” Buttons “Stay”, “Save and leave”, “Leave without saving”. Save failure keeps modal/session, showing “Save failed. Your current life is still open.” and “Try again”, “Choose another slot”, “Stay”. Slot choice uses Review overwrite protection.

Default focus Resume; Up/Down follows action order, Escape/B/Menu resumes outside modal. Settings returns to Pause, still frozen. No selected action state; hover/focus/error follow §1. Immediate freeze, panel crossfade only.

**Acceptance:** game clock stays frozen through Settings and dialogs; failed save cannot trigger departure; full slots allow replacement choice without abandoning current life. Successful save updates timestamp only after completion.

### Load

Purpose: inspect a record before loading. Wide panel 1200×820 maximum, 40% three-record list / 60% details; narrow available viewport, list then selected details in scroll body, sticky Back/Load footer. Heading “Load a life”; introduction “Choose a record, then load it to continue.” Records: “Slot {Slot}”, “{Name} · Age {Age}”, “{Date} · {Location}”, “Saved {Time}”, status “Draft” / “In progress”. Detail adds “Last action: {Action}”, “Current commitments”, or “No commitments recorded”; missing legacy metadata says “Not recorded”. Empty slot “Empty slot”; no records “No saved lives yet.”

Rows are selectable, including empty/damaged records for explanation. Initial selection most recent readable record, otherwise first; activation selects only. Separate “Load” action becomes “Continue draft” for drafts. Empty action disabled: “Choose a saved record to continue.” Damaged: “This record couldn't be read.” Unsupported: “This save needs a compatible game version.” Recovery: “An earlier recovery copy is available from {Time}.” Button “Load recovery copy”; confirmation “Load the recovery copy for {Name}?” / “Progress after {Time} will not be restored.” Buttons “Cancel”, “Load recovery copy”. After recovery: “Loaded a recovery copy. The original record has been kept.” Never report recovery as corruption failure or delete original silently.

Current `DescribeSaveSlot()` returns opaque text. **core: needed** `FPWSaveSlotView { SlotIndex, Revision, Status, FullName, Age, GameDateText, LocationText, SavedAt, LastAction, Commitments[], ResumePoint, bCanLoad, RecoverySavedAt, bCanRecover, MessageCode, MessageArgs }`; read-only inspection must not replace current state. Existing `LoadFromSlot` preserves draft on failure; adapter must extend this to live sessions. Successful load routes via `EPWResumePoint`, with existing Appearance value mapping to Household. Loading during a dirty session uses Pause departure protection.

Focus: record list → detail/recovery control → Back → Load; Up/Down moves list focus, A selects, Tab/F6 reaches action. Double-click never loads. Pending load disables repeat action, shows “Loading…”; errors remain beside action. Detail crossfade only.

**Acceptance:** select every row without changing current life. Test empty, draft, live, incompatible, corrupt and recovery records. Failed load retains session; successful draft resumes correct step. Recovery never consumes an ordinary life slot or erases its source.

## 8. Phase 3 — Settings

Purpose: functioning comfort controls. Wide panel 1200×840 maximum, 22% category rail / 78% settings; narrow category selector above full-width scroll body. Heading “Settings”; categories “Display”, “Audio”, “Controls”, “Accessibility”. Footer “Restore category defaults”, “Back”, “Revert”, “Apply”. Defaults affect current category's pending values only. Apply/Revert disabled when clean. Back while dirty asks “Keep these settings?” with “Keep editing”, “Discard changes”, “Apply changes”.

| Category | Exact labels, choices and behavior |
|---|---|
| Display | “Window mode”: “Windowed”, “Borderless”; “Vertical sync”: “Off”, “On”. Use supported engine values; no exclusive resolution picker in this phase. |
| Audio | “Master volume”, “Music volume”, “Effects volume”, “Dialogue volume”: 0–100%, steps 5; “Play sample”. Preview pending mix on sample; Revert restores applied mix. |
| Controls | “Mouse sensitivity”: 0.2–3.0, steps 0.1; “Controller sensitivity”: 0.2–3.0, steps 0.1; “Invert vertical look”: “Off”, “On”; “Camera”: “First person”, “Third person”. Default camera first person. |
| Accessibility | “Text size”: “100%”, “125%”, “150%”; “Reduced motion”: “Off”, “On”; “Subtitles”: “Off”, “On”; “Speaker names”: “Off”, “On”; “Notice duration”: “7 seconds”, “14 seconds”, “Until dismissed”. Preview sentence “A new school day begins in Kolkata.” |

Text size previews immediately and preserves focus/scroll; Revert restores previous size. Reduced motion immediately stops menu camera drift, translation and smooth scrolling. Accessibility copy: “These settings cost no points and do not change achievement eligibility.” Applied preferences persist across sessions and are independent of saves.

Existing `UGameUserSettings` supplies window mode and vertical sync. Current `PerfectWorld.Input.MouseSensitivity` is stored but not consumed. **core: needed (engine settings adapter)** `FPWSettingsView { Applied, Pending, Capabilities, bDirty, Error }`, each snapshot exposing `WindowMode, bVSync, MasterVolume, MusicVolume, EffectsVolume, DialogueVolume, MouseSensitivity, ControllerSensitivity, bInvertY, CameraMode, TextScale, bReducedMotion, bSubtitles, bSpeakerNames, NoticeDuration`. Wire consumers before enabling. Unavailable control remains disabled with “Unavailable in this build.” Apply failure “Settings couldn't be saved. Your previous settings are still available.” Keep pending edits for retry. Display apply opens “Keep these display settings?” with “Revert”, “Keep”; default Revert; “Reverting in {Seconds} seconds.” Timeout 15 seconds on real time, including lost focus; restore last working mode.

Focus: category → controls top-down → footer; arrows adjust one discrete step, Enter/A opens choice menus. F6 moves regions; category selection preserves pending changes. Selection checks applied/pending value, not keyboard focus. Numeric changes immediate; sample playback only on activation.

**Acceptance:** settings audibly/visibly affect consumers and survive restart; unsupported controls cannot pretend success. Test display timeout, persistence failure, dirty Back, controller adjustment and 150% text on every screen. Reduced motion suppresses all specified movement.

## 9. Phase 3 — In-world notices

Purpose: acknowledge events without taking control. At 1080p use a 480-unit maximum stack above the lower-right safe area, 48 margin; at 720p maximum 360, 24 margin. Keep clear of subtitle and interaction regions using HUD reserved rectangles; if crowded, place above subtitles. Maximum two visible, further notices queued. Text wraps without truncation; no full-screen scrim.

Exact templates: success “Saved to Slot {Slot}.”; information “{Title}” / “{Message}”; recovery “Recovery copy loaded.” / “Your original record has been kept.”; failure “Couldn't save.” / “Your current life is still open. Open Pause to try again.” Optional action “View details”; manual close “Dismiss”. Gameplay notice bodies come from authored event records; no raw debug/error strings. Use icon plus text for information, success, recovery and error; care is never a warning severity.

**core: needed** `FPWNoticeView { Id, Severity, Title, Body, ActionLabel, ActionId, bRequiresAcknowledgement, CreatedAt }`; engine adapter owns queue, duration and dismissed history. Existing `SPWFrontEndRoot::ShowToast(FText)` cannot represent this. Routine notices last selected 7/14 seconds; errors/recovery/action-required notices persist until dismissed. Repeat identical event IDs coalesces; unique events queue, never overwrite an unread error. Pause holds timers. Preserve notices in session “Recent notices”, accessible through Pause after the main actions, newest first; empty text “No recent notices.”

Notices never steal focus. F8/controller View opens Recent notices in a modal; focus newest, Up/Down moves entries, A opens details, B closes and restores world input. Action and Dismiss receive hover/focus; selected state only applies to history row; unavailable action disabled with “Details are unavailable.” Fade 160 ms without slide; reduced motion instantaneous.

**Acceptance:** trigger three events plus save failure while moving; movement remains uninterrupted, error remains readable, all events appear in history. Test subtitles, enlarged text, Pause timer suspension, dismissal and keyboard/controller detail access.

## 10. Delivery boundary

Accept phase 2 after previews/history and its three screens pass their checks. Phase 3 additionally requires the save/session/validation/settings/notice contracts and failure-path checks. Test packaged builds at both target resolutions. The implementing engineer records verification separately; this specification claims no implementation.
