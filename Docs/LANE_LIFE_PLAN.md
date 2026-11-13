# Lane life plan — design (gpt-6-astra, high effort, read-only, 2026-10-13)

The owner's request of 2026-10-12: a more visual front end ("pentagrams or other things"), workable houses, talkable NPCs each with a small AI doing their work, and a scale check. The owner chose gpt-6-astra to lead planning. The companion implementation sequence is `LANE_LIFE_IMPLEMENTATION.md` (gpt-6.1-sol). Unverified until built and played; the scale table reflects the source at `76570ec`.

**The interpretation is broadly right. I would change three things: use an original fivefold instrument motif instead of a literal pentagram; treat “all houses workable” as a tracked destination with staged delivery; and build NPC behaviour by integrating the existing core modules.**

The walking character is already implemented. The next character work is verification, interaction, animation and appearance integration—not replacing a flying pawn.

This review is based on `integrate/ui-phase1-lane-week`. No files were edited and no builds or playtests were run. A separate `gpt-6-astra` design critique informed the recommendations.

**A. Front-end visual language — “Fivefold Aperture”**

The front end should suggest a whole life opening into an unusual world. Keep the Kolkata street, readable typography and amber controls dominant. Ornament supplies mystery around that composition.

A literal pentagram would make the game look more exclusively occult than its premise warrants. Fivefold geometry gives the owner the desired visual richness without promising that character creation is a supernatural initiation.

**Construction specification**

Use one procedural Slate ornament family, built from polylines, arcs and small boxes. All dimensions below are logical Slate units before DPI scaling.

| Motif | Geometry and proportions | Treatment |
|---|---|---|
| **Aperture** | Five identical annular sectors, centred at angles `−90° + 72°k`. Each sector spans 48°, leaving five equal 24° gaps. Outer radius `R`; inner radius `0.88R`. Join inner and outer arcs at their ends. Keep the centre empty. | Main outlines 1.5 units; inner outlines 1 unit. All five sectors have identical area, opacity and motion. |
| **Open points** | Inside each sector, one disconnected chevron: tip at `0.76R`, two ends at `0.59R`, angular offsets ±12° from that sector’s centre. Never connect tips across the centre. | 1.5-unit stroke. Suggests five outward points without drawing a pentagram or intersecting star. |
| **Calibration ticks** | Five ticks per sector, at angular offsets −16°, −8°, 0°, 8°, 16°. Each runs from `1.03R` to `1.07R`; the central tick reaches `1.10R`. | 1-unit stroke. These are measurement marks, not invented writing or borrowed “runes.” |
| **Corner brackets** | Two perpendicular 12-unit segments, separated from a panel corner by 8 units; a second 4-unit line sits 4 units inward. | Use sparingly on the title plaque and review summary. Never outline every card. |
| **Motes** | Ten 2–3-unit squares distributed in five equal angular groups outside the aperture, between `1.15R` and `1.5R`. | Decorative only; never cross text, focus rings or controls. Omit at narrow layouts. |
| **Small seal** | At diameters below 48 units, retain only five short equal arcs and the central page pictogram. | Remove ticks and inner chevrons rather than shrinking them into noise. |

Use the existing [PWStyle palette](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/UI/PWStyle.cpp:95):

- `Background #101A1D` and `Panel #18262A`: existing surfaces.
- `Divider #415359`: quiet construction lines, 35–55% opacity.
- `TextSecondary #BBC8C7`: secondary ornament, 20–35%.
- `Accent #E9B568`: principal ornament, 25–45%; small foreground seals may reach 70%.
- `TextPrimary #F4EEDF`: sparse highlights.
- Keep `Focus #9CDCDD`, `Success` and `Error` reserved for their interface meanings.

Do not assign amber to Agni or give each Tattva a different brightness. This family represents the **existence of five categories**, not the player’s affinity distribution. An equal-looking emblem must never be labelled “20% each.”

Layer order: street → existing shade → decorative geometry → opaque reading surfaces → text and controls → focus/error indicators. Ornament is non-interactive and excluded from accessible reading order.

**Placement and motion**

| Screen | Placement and behaviour |
|---|---|
| **Title** | One aperture in the right-hand third, diameter roughly 340–460 units, clamped to available space. Keep the street’s main sightline visible. Reveal all five sectors together over 800 ms, cubic ease-out; title and controls remain immediately usable. Rotate only the outer calibration ring, one revolution per 90 seconds, linear. |
| **Household** | A 32-unit seal beside the step label; centre pictogram is a simple open doorway. A larger stationary fragment may sit in unused preview space. Identical ornament for family and care origins. |
| **Traits** | Same five-part seal, centre pictogram three equal horizontal ledger lines. No sectors brighten as points are spent. Budget completion retains its ordinary check and text. |
| **Identity** | Centre pictogram is a circle over an open shoulder bracket. Appearance, names, pronouns and language never alter the surrounding geometry. |
| **Stream** | Centre pictogram is three equal parallel paths. Science, Commerce and Humanities receive identical ornament and prominence. |
| **Review** | Centre pictogram is a doorway above a short horizon. The full stationary aperture accompanies “Your first morning,” away from the summary text. |
| **Start Life** | After a successful start-and-save transaction, all five sectors expand together from `R` to `1.12R` and fade over 450 ms, cubic ease-out. A restrained opening gesture, with no flash, elemental colour or power-awakening sound. Never delay a ready world to finish it. |
| **Loading** | Small seal beside truthful loading text. If progress is known, use a separate progress bar; the five sectors are not loading stages. Never fabricate percentage progress. |
| **Pause** | Stationary 48-unit seal and corner brackets. No rotation, particles or reveal loop. The player should feel that time has stopped. |

Creation page crossfades remain 160 ms; modal transitions remain 180 ms. Do not add translation to these.

**Reduced motion means stationary immediately:** no ring rotation, motes, camera drift, tracing animation, expansion or page translation. Show the completed emblem at once. This must include the existing title camera drift in [PerfectWorldFrontEndHUD.cpp:86](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldFrontEndHUD.cpp:86), not merely the new decoration.

The Start Life effect has a functional prerequisite. The current HUD calls `StartLife()`, attempts a save, then enters the world even when saving fails. Resolve that mismatch with the front-end specification before celebrating success. See [PerfectWorldFrontEndHUD.cpp:180](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldFrontEndHUD.cpp:180).

**Three rejected alternatives**

1. **A complete pentagram inside ritual circles:** too strongly suggests occult practice as the game’s central activity; rotation could also pass through the explicitly unwanted inverted presentation.
2. **Five saturated elemental emblems:** encourages players to read creation choices as affinity selection and risks giving fire or space more visual prestige.
3. **Dense animated ornament on every panel:** competes with Bengali text, focus and budget information, and makes ordinary decisions feel like magical rituals.

The bible does not require erasing religion from lived settings. It requires context and review. This interface uses no deities, scripture, copied yantras, ritual objects or national tricolour shorthand. The finished geometry still deserves a visual cultural review; calling something “original” does not prove that it has no unintended resemblance.

**B. Living lane — twelve people with recognisable lives**

Use the existing bible’s cast. Add four residents to support the street’s everyday functions. Do not replace Farida with a generic tea vendor or introduce duplicate classmates.

The desired feeling is: **“I know who belongs here, I notice when their routine changes, and they remember something specific about me.”**

The following are proposed authored beats, not claims about real Kolkata working hours. School attendance uses the existing fictional timetable: gate opens 09:30, assembly 10:00, dismissal 15:50. Travel occupies explicit intervals between activities.

| Person | Block identity | Day, workplace and behaviour |
|---|---|---|
| **Farida Qadri**, Shanti Chai proprietor; existing | 158 cm; sturdy frame, tied-back hair, cream shirt, terracotta apron; deliberate two-handed carrying. | 05:30 preparation; 06:00–11:30 serving; midday closing, supplies and rest; 15:30–19:00 serving; cleaning and home afterward; sleep from 22:00. **Prepare → serve → wash → break → close.** Concern: keeping opening hours reliable. |
| **Mira Sen**, Commerce classmate; existing | 160 cm; straight bob, square satchel, navy uniform; checks a small list before moving. | Morning study; travel for school; school 10:00–15:50; stationery counter Tuesday/Thursday 16:30–18:00; other afternoons her own plans; home and sleep from 22:30. **Study → travel → class → counter/rest.** Concern: choosing her time without abandoning family work. |
| **Ishan Sheikh**, Science classmate; existing | 173 cm; slight build, short curls, cross-body notebook bag; pauses to write. | Morning bus arrival and lane walk; school; selected evenings at the maidan recording stories; home study; sleep from 22:30. **Travel → class → observe/write → socialise.** Asking permission to record is part of his behaviour. |
| **Tara Das**, Humanities classmate; existing | 168 cm; long braid, athletic frame, light running jacket outside school; purposeful gait. | 06:30 practice; breakfast and school; sibling pickup after dismissal; home tasks; occasional evening walk; sleep from 22:00. **Run → class → pickup → home/rest.** An early departure occurs only through an explicit schedule exception. |
| **Amrita Basu**, teacher; existing | 163 cm; low bun, rectangular glasses, blue-grey clothing, document bag; measured gestures. | Morning preparation; arrival before assembly; school; marking until 17:00; short lane errand; home preparation and leisure; sleep from 22:30. **Prepare → teach → mark → travel → rest.** Concern: finding enough individual time for students. |
| **Dr. Ananya Chatterjee**, physician; existing | 166 cm; cropped hair, cream coat over dusty blue, compact bag; attentive upright posture. | Alternating morning or evening clinic sessions; records and referrals outside appointments; meal/rest interval; home by late evening; sleep from 23:00. **Consult → record → break → travel.** Street conversation never exposes patient information. |
| **Amit Roy**, NGA field officer; existing | 178 cm; broad shoulders, short hair, muted work shirt and ID clip; checks written notices. | Day shift at the Annex, with an authored lane inspection and meal break; home in evening; sleep from 22:30. **Desk → inspection → report → off-duty.** A callout is a priority exception, not perpetual patrol. |
| **Household adult / care worker**, existing conditional role | Appearance comes from the resolved origin. For a staff-caregiver prototype: 162 cm, silver-streaked hair, sage overshirt and key pouch. | Breakfast support; template-specific work or staff shift; evening household coordination; handover or personal time; sleep outside duty. **Home task → work → shared time → rest.** Care must not imply one employee works around the clock. |
| **Anindita Sen**, stationery proprietor; proposed, Mira’s parent | 154 cm; short wavy hair, plum blouse, paper-coloured apron; rests one hand on the counter while listening. | 07:30 stock check; 08:00–13:00 counter; meal closure; 15:00–19:30 counter; reconciliation, home; sleep from 22:30. **Open → serve → restock → reconcile → close.** Concern: making the shop manageable without automatically relying on Mira. |
| **Debashis Pal**, repairer; proposed | 171 cm; sturdy, greying cropped hair, ochre shirt, tool belt; slow inspection before handling a part. | 08:00 workshop setup; repair bench until noon; meal and appointments; afternoon collection window; closes 18:30; personal projects and home; sleep from 23:00. **Inspect → repair → test → explain → close.** Concern: promising realistic completion times. |
| **Leela Dutta**, taxi driver; proposed | 176 cm; long plait, blue overshirt, glasses and shoulder bag; relaxed stance beside the parked taxi. | Early driving shift; 10:30 lane break; further work; 14:00 meal/rest; late-afternoon shift; home from 20:00; sleep from 22:00. **Off-map shift → park → break → depart.** Start with the existing taxi; an auto-rickshaw is a later vehicle asset. |
| **Subrata Dey**, cooperative caretaker and neighbour; proposed | 181 cm; narrow build, thinning grey hair, sage workwear, folding clipboard; slightly stooped reading posture. | 06:30 common-area checks; morning maintenance; midday home/rest; 16:30 noticeboard and agreed repairs; evening leisure; sleep from 21:30. **Inspect → sweep/repair → record → rest.** Concern: residents agreeing on shared maintenance. |

Author all seven days, including meals, travel, leisure and overnight sleep. Weekend variants must replace school/work blocks deliberately: students gain personal time; Basu has a non-school day; businesses have posted shorter hours or a rest day; clinic and shift workers have explicit rotas. No named person should fall into an unexplained all-day idle state.

Keep every pair distinguishable in at least two silhouette attributes. Complexion, religion, language and perceived class must not function as behavioural shorthand.

**Three sample lines each**

The relationship-dependent lines require the named remembered event; they should never play merely because a numerical score is high.

| Person | Earlier encounter | Later/time-dependent | Remembered relationship |
|---|---|---|---|
| Farida | “The water is nearly ready. There’s room beside the bench.” | “I’m washing the last cups. I’ll open again tomorrow morning.” | “You asked before moving the bench. Thank you—it kept the doorway clear.” |
| Mira | “I’ve got time to walk with you as far as the corner.” | “I’m at the counter today. We can talk while I finish this list.” | “You brought the notes back when you said you would. Want to compare the last question?” |
| Ishan | “I’m writing down how this street changes in the rain.” | “I put the notebook away. I’d rather just sit for a bit.” | “You said that part was private. It isn’t in my notes.” |
| Tara | “One more lap, then breakfast. You can join me if you like.” | “I have a pickup to make. Can we finish this tomorrow?” | “Thanks for keeping that conversation between us.” |
| Basu | “Bring the question you got stuck on, even if you haven’t finished.” | “I’m heading home now. We can make time at school tomorrow.” | “You told me the plan wasn’t working. That helped us change it.” |
| Chatterjee | “The clinic opens shortly. The hours are on the board.” | “I’m off duty now. The notice lists where to ask for help.” | “You asked before helping. That gave the person a choice.” |
| Amit | “I’m checking whether the public notice is still readable.” | “There’s no new closure notice. I’ll update the board if that changes.” | “Your report included the place and time. That made it useful.” |
| Household adult / care worker | “Breakfast is ready. Tell me what time you expect to be back.” | “We still need to agree who does the evening jobs.” | “The arrangement you suggested worked yesterday. Shall we keep it?” |
| Anindita | “The plain exercise books are on the lower shelf.” | “I’m checking the stock before I close.” | “Your labels saved me a second count. I’ve kept them.” |
| Debashis | “Leave it on the clear part of the bench. I’ll have a look.” | “I’m testing this before I call it finished.” | “You described what happened before it stopped. That helped.” |
| Leela | “I’m taking a break before the next shift.” | “I’m finished for today. The stand is farther along.” | “You remembered the bag I left behind. Thank you.” |
| Subrata | “I’m clearing this edge so people can pass.” | “The repair list is up. Add the location if I missed something.” | “We tried your suggested time. More neighbours could make it.” |

**What “small AI” means**

Use one shared behaviour framework with different authored schedules, work anchors and animation sets.

Core owns identity, schedule resolution, availability, conversation choices, relationship events, permissions and thread consequences. Unreal owns the visible route-following, avoidance, gaze, gestures and contact animation.

The foundations already exist:

- `IdentityRegistry`: authored identities and persistent first references.
- `Schedules`: stays, travel, idle, priority exceptions and off-screen tallies.
- `Relationships`: remembered events and first meaningful acts.
- `Threads`: timed escalation and authored expressions.
- `TimeContract`: paused conversation and declared costs.

A significant current-state correction: [WorldState already contains DayLoop](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorldCore/Public/WorldState.h:77), and portable world saves are schema 7. Some comments and roadmap entries are older. Identity, relationships, NPC schedules and threads still need world integration. No dedicated conversation runner or people catalogue appeared in the searched core.

Use these shared transitions:

| State | Entry and exit |
|---|---|
| **Work** | Schedule selects a work role and anchor. Presentation cycles through a few purposeful actions. Exit at a schedule boundary, break or accepted conversation. |
| **Travel** | Core selects destination and travel interval. Presentation follows its route. Arrival leads to the scheduled role. |
| **Idle/rest** | Authored rest or a genuine gap. Sit, read or observe; avoid endless purposeless pacing. |
| **Chat** | Accepted talk pauses world time. A worker finishes a short safe gesture before attending. Closing re-evaluates the schedule after any declared charge. |
| **Sleep/private** | Person remains at a valid home destination; ordinary street talk is unavailable. |
| **Yield/blocked** | Local avoidance yields or uses a passing space. Persistent obstruction leads to an explicit delayed/replanned outcome, never a hidden teleport. |

Do not turn every sweeping stroke or cup placement into persistent simulation. Record meaningful work completion at the appropriate boundary.

**Resolve the clock–walking conflict explicitly.** At 1.4 m/s, traversing the 88 m lane takes about 63 real seconds—and therefore 63 game minutes. Author local travel intervals to fit this contract, with nearby work/home anchors and arrival windows. Do not give off-screen NPCs shorter travel merely because nobody watched them. Reconstruct a loaded traveller along the same journey.

Ordinary avoidance remains presentation. If obstruction is allowed to change attendance or another persistent outcome, that result must enter through a bounded, recorded core command. Frame-dependent arrival callbacks must not silently decide someone’s life.

**Conversation UX**

- Acquire targets using the existing contract: 2 m reach, forgiving cone, stickiness and target cycling. Use a common interaction origin/reach rule so third-person camera distance cannot extend reach.
- Prompt: **“E / X · Talk to Farida”**, with separate “Buy tea” where applicable. Reserve controller A for the existing jump binding in free roam.
- Converse at approximately 1.2–1.6 m where space permits. Do not drag the player into position.
- Keep the current camera by default. Optional framing releases on any look input.
- Use a flat lower-screen panel: speaker name, readable subtitle, up to three choices and an always-visible **Leave** action. At narrow widths, stack and scroll.
- Greetings and directions cost zero. A longer exchange says, for example, **“Talk about the rota · 3 minutes”** before commitment. Reading consumes nothing; committed costs apply once when the conversation resolves.
- Escape/B leaves at any line. Opening, leaving and reopening cannot repeat rewards or erase a committed choice.
- Sleeping, occupied or off-duty people can decline politely. “Everyone talkable” means each has authored interaction, not permanent availability.

Begin with two existing threads: `thread.borrowed_notes` and `thread.evening_rota`. Add Farida’s existing stall thread after ordinary service and social conversation work. The street should have pleasant days without manufacturing twelve simultaneous crises.

**C. Houses — open a convincing cluster, then complete the lane**

Every player-facing lane building needs a stable building ID, ownership/use, access rules and a tracked interior status. Track these separately:

1. Interior constructed.
2. Interior furnished and usable.
3. Resident/workplace integrated.
4. Player currently permitted to enter.
5. Verified in both cameras.

A private locked home can be complete. A locked door hiding missing geometry remains unfinished work.

The generated near row contains nineteen side buildings plus two end-closing blocks. Those closing blocks are broad mixed-use masses, not necessarily individual houses. Freeze their authored identities before attaching saves or residents to generated array indices.

**First cluster**

The current deterministic layout provides practical candidate footprints:

| Location | Candidate frontage | Interior and reason |
|---|---|---|
| **Opening home** | −Y side, approximately X −300 to 700 cm | First proves the actual promise: waking, leaving and returning. Entry/utility space; living/eating room; sleeping/study room; kitchen; washroom. Bed, desk, chair, storage, meal surface, bag hook and one personal object per room. Resolve the actual origin rather than assigning this family flat to everyone. |
| **Shanti Chai Corner** | +Y side, approximately X 600 to 1300 cm; 120 cm setback | First public interior and first workplace. Covered seating, service counter, preparation/washing space and storage. Customers enter seating; the preparation area has an ordinary staff boundary. The setback helps retain a usable footpath. |
| **Stationery shop** | +Y side, approximately X −100 to 600 cm | Connects Mira’s school and family life. Sales room, stock room, counter with side access, shelves and packing desk. It can reuse the public-shell construction while feeling different. |
| **Neighbour/caretaker flat** | −Y side, approximately X 2600 to 3300 cm | Proves another private home with permission and visiting hours. Sitting room, kitchen, sleeping room, washroom and shared landing; different possessions and furniture arrangement. |
| **Repair workshop** | A surveyed wide shutter frontage in the same cluster | Bench room, collection counter and secure storage. Gives work animation and tool interaction a purpose. Confirm circulation before choosing the final footprint. |

These are proposed assignments, not existing named locations.

Institutional care needs a genuinely resolved care opening: personal sleeping/storage space, shared study/eating space and an available staff member. It must receive the same lighting, navigability and interaction care as a family home.

**Construction and door feel**

Replace solid masses with floors, ceilings and wall segments around openings. Do not remove collision from the whole building to let the player through.

Use the existing 25 cm wall reference; 90 × 210 cm **clear** door openings; corridors at least 120 cm clear; and furniture layouts that preserve turning and camera space. Reserve stairs and upper-floor footprints now.

A door interaction should be:

- One press to open; approximately 0.45 seconds for an ordinary hinged leaf.
- Handle/hinge sound, a small hand gesture when available, and a clear opening.
- Sweep checked against people. If obstructed, stop and explain; never push a character through furniture.
- Close only when its sweep is clear.
- Open/closed/locked/permission state persists through the core.

A 90 cm painted rectangle is not a 90 cm usable opening once jambs, leaf thickness and swing are considered.

For lighting, follow the asset contract: warm practicals or cool work lights, readable ambient fill, controlled exposure and ceilings everywhere. Use tightly bounded lights so the existing unshadowed street bulbs do not illuminate rooms through walls. Keep the per-view shadow-light budget.

Closed doors need distinct meanings:

- **Closed, accessible:** visible handle; “Open.”
- **Private, occupied:** “Knock”; resident may answer at the threshold.
- **Business closed:** posted opening hours and “Closed until …”.
- **Locked:** explicit explanation based on known information; no unexplained failed interaction.
- **Outside the current delivered area:** honest boundary treatment. Do not imply a working room exists behind it.

Ground floors are the first delivery, not the definition of final completion. Subsequent stages add every reachable frontage, then upper dwellings and usable roof/stair destinations.

**D. Scale corrections**

All measurements below are centimetres. “Contract” means this project’s presentation contract; proposed circulation targets are design targets, not claims of building-code certification.

The original 200 cm eye height and undersized door leaves are already corrected. The remaining problems include **clearance, floor datums and collision**, which nominal mesh sizes alone do not catch.

| Item and source | Current | Target / correction | Why |
|---|---|---|---|
| **Building structure** — [Buildings.cpp:177](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneBuildings.cpp:177) | Entire footprint is a solid wall-colour box. | 25 cm reference walls, actual openings, floor and ceiling slabs. | Current houses have no traversable interior volume. |
| **Ground-floor door and step** — [Shops.cpp:144](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneShops.cpp:144) | Door spans W 15–225; step reaches W 28. Measured above the step, only 197 cm remains. Step is currently decorative/non-colliding. | Choose finished floor/landing first, then place the 210 cm clear opening above it. At floor W 28, opening top becomes W 238. | The corrected leaf still uses an inconsistent threshold datum. |
| **Door frames** — [Buildings.cpp:41](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneBuildings.cpp:41), [Shops.cpp:144](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneShops.cpp:144) | “Frames” are filled rectangles behind door decoration. | Separate jambs and lintel outside a measured 90 × 210 clear aperture. | An exterior dimension is not clearance. |
| **Storey type and datum** — [Blocks.h:22](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneBlocks.h:22), [Buildings.cpp:134](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneBuildings.cpp:134) | All storeys use 300; upper levels are measured from road datum, while ground-floor features use the footpath. | Keep flats at 300; use 360 for buildings explicitly authored as older houses/school. Derive all levels from finished-floor elevations. | Avoid a blanket rescale; fix the building types and vertical relationships. |
| **“75 cm plinth”** — [Buildings.cpp:178](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneBuildings.cpp:178) | A thin fascia from W 15 to 75: 60 cm tall. It is not a raised interior floor. | Treat it as decorative basework, or author a real raised floor with a corresponding entrance. | Raising the floor to 75 without rebuilding the doorway would create another height error. |
| **Window opening definition** — [Buildings.cpp:24](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneBuildings.cpp:24), [Shops.cpp:164](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneShops.cpp:164) | Outer dark rectangle 120 × 135; inset glass 106 × 121. No wall opening. | Preserve 120 × 135 as the structural opening; document the smaller glazing separately. Sill remains 75 above the actual room floor. | The audit currently conflates representation with an opening. |
| **Footpath at stalls** — [Shops.cpp:102](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneShops.cpp:102) | A stall barrier projects 92 cm. On a zero-setback 150 cm footpath, only **58 cm** remains. | Recess counters or widen their forecourts to maintain a continuous 120 cm route; provide wider passing areas. | The player’s capsule is 68 cm across. This is physically impassable without stepping into the road. |
| **Pots and trees** — [Shops.cpp:59](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneShops.cpp:59), [Props.cpp:114](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneProps.cpp:114) | Pots project 48 cm; trees sit within pedestrian space. Placement has no clearance reservation. | Place them after reserving routes, doors, conversation positions and passing spaces. | Individually plausible props can collectively obstruct movement. |
| **Awning headroom** — [Shops.cpp:129](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneShops.cpp:129) | Bottom W 206 minus pavement W 15 = **191 cm**. | At least 210 cm above walking surfaces; prefer 220 for this repeated street element. | Clears the current 165 cm body but intersects the contract’s 195 cm height extreme. |
| **Balcony/roof collision** — [Buildings.cpp:47](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneBuildings.cpp:47), [Blocks.cpp:49](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneBlocks.cpp:49) | Balcony concrete and rails have no collision. Rear/side roof parapets reach 90; the front alone gets coping to 100. | Before access: walkable slab and matching rail/parapet collision; consistent authored protective height. | Decorative balconies must not become fall-through destinations. |
| **Roof tank** — [Buildings.cpp:94](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneBuildings.cpp:94) | Rotated 130 cm square produces an approximately **184 cm** horizontal envelope. Body is 110 high; base 40; lid reaches W H+160. | Correct the audit. If retaining its roughly 110 × 120 reference, construct to that final envelope; otherwise declare a larger fictional tank. | Audit wording “130 across, 150 tall” does not describe the generated bounds. |
| **Scooter** — [Props.cpp:149](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneProps.cpp:149) | Approximately 190 × **68** × 112; wheelbase **140**; both wheel blocks 50 diameter. | Either declare a fictional scooter, or select one reference model and match its dimensions consistently. | “Activa class” currently mixes approximate dimensions. A Honda-produced 6G brochure lists 183.3 × 69.7 × 115.6 and a 126 cm wheelbase. [Honda brochure](https://www.bhagwatihonda.com/pdfs/Activa6GBrochure.pdf) |
| **Running reference** — [PlayerCharacter.h:32](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Public/PerfectWorldPlayerCharacter.h:32) | Run speed 4 m/s; asset-pipeline procedural running reference is 3.5 m/s. | Deliberately align the movement and animation reference; my initial target is 3.5 m/s. | This is a tuning mismatch, not a broken scale calculation. |
| **Navigation geometry** — [Blocks.cpp:171](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneBlocks.cpp:171) | Navigation contribution disabled for all lane components. | Provide explicit walkable/collision navigation geometry for shells and routes. | Adding NPC pathfinding alone cannot make the current street navigable. |
| **Camera and boundary collision** — [Blocks.cpp:182](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneBlocks.cpp:182), [Barriers.cpp:21](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneBarriers.cpp:21) | Props use Pawn-only barriers; cameras pass through them. Side alleys end at invisible barriers. | Solid visible objects need appropriate Camera/Visibility collision; accessible boundaries need visible architecture or gates. | Current behaviour falls short of the presentation contract even where object dimensions are plausible. |

**Already corrected or acceptable at this stage**

| Item | Current finding |
|---|---|
| Player | Radius 34, body/capsule height 165, eye 155; spawn uses road height plus capsule half-height and settling clearance. Keep these. |
| Road | 500 cm width. GroundZ −200 is a coordinate choice, not a height defect. |
| Footpath/kerb | Walkable top 15 cm above road. Decorative kerb reaches 17; a 2 cm visual/collision difference, not a major mismatch. |
| Upper windows | Nominal 120 × 135, sill 75, chhajja 45. Preserve while converting to real openings. |
| Door leaves | Roof, balcony and residence leaves now 90 × 210. Do not repeat the old 70 × 175 finding. |
| Shutters/counter | Shutter nominal height 240 above footpath; counter top 90 above it. Correct dimensions, but stall access and circulation remain unresolved. |
| Taxi | Approximately 436 × 165; body roof 146, roof sign reaches 160. The “160 high” figure includes the sign. No further rescale justified from the project reference alone. |
| Handcart | Deck 180 × 90, top 60; roughly 240 long including handles. Do not mistake deck dimensions for total dimensions. |
| Poles/wires | Pole 900 from road datum; main attachments 849. Longest main span sags about 63, leaving roughly 786 above road. Retain as block references; this is not an electrical-clearance certification. |
| Stairs | No usable stair flight exists to audit. New stairs should use the 17.5 riser/25 tread reference, with equal risers adjusted to reach the actual landing exactly. |
| Auto-rickshaw/bicycle | Neither exists in the lane. Do not report a current mismatch. For a future auto, choose a specific reference: one Bajaj RE brochure gives approximately 265.8 × 130 × 170, so 140 cm width is not universal. [Bajaj specification](https://www.bajajauto.com/en-bd/-/media/globalbajajauto/common-media/brochure/3wheelers/bajaj-re-4s-lpg-without-door-brochure.pdf) |

**E. Priorities, staging and acceptance**

Deliver a visibly improved front end quickly, then a small connected piece of ordinary life. Expand from that working example.

| Stage | Deliverable | Acceptance criterion |
|---|---|---|
| **1. Ornament and comfort** | Fivefold Aperture on title and creator; one reduced-motion preference reaches existing and new motion. | At 720p/1080p and 150% text, controls remain readable and reachable. All five sectors are equal. Reduced motion stops camera drift and every decorative animation immediately. |
| **2. World entry, clock and reliable continuation** | Connect the existing portable world snapshot to Unreal; transactional Start Life; free-roam/pause clock modes. | Start failure preserves the draft. Successful start enters the resolved origin. Save/load restores time, position and camera. Sixty real seconds advances sixty game minutes; reading/pause advances zero. Then enable the Start Life flourish. |
| **3. One home and one adult** | Walkable furnished shell, working door, targeting, household/care adult and one remembered exchange. Covers the relevant M1-17–21 work. | In both cameras: enter, close door, sit, read, talk and leave. No clipped ceiling or furniture. Door and choice survive reload. Care opening passes the same checks. |
| **4. First public cluster** | Tea shop, stationery shop, Farida, Mira and Anindita; usable footpaths and deterministic travel. | Observe opening, service, a break and closing. Talk to all three. Follow Mira without teleportation. A blocked doorway cannot trap the player or NPC. Save during travel and resume coherently. |
| **5. Twelve-person week** | Complete cast, full-week schedules, relationship integration and two ordinary-life threads. Aligns with M2-14/18 and M3-12. | Every person has reachable work/home anchors and valid seven-day coverage. Manual and compressed runs produce equivalent persistent results. Remembered dialogue survives reload; repeated greetings do not farm relationships. |
| **6. Every lane ground floor** | Complete the frontage inventory using varied residential/public kits. | Every reachable entrance has an implemented destination or an explicit tracked remaining task. Each completed interior has ceilings, furniture, light, ownership, access rules and a useful interaction. Locked status cannot conceal an untracked missing interior. |
| **7. Upper dwellings and final lane gate** | Stairs, remaining households and permitted roof destinations; full comfort/performance review. | Every house in the agreed lane inventory is accounted for. Walk and camera tests pass at supported proportions. A cooked morning-to-night run demonstrates persistence and acceptable performance. |

Run these as bounded implementation steps, normally one to three files or one content family at a time. Record **implemented**, **tested in core**, **built in Unreal**, **played**, and **cooked/verified** separately.

The main risks and decisions are:

- **Determinism:** NPC presentation must not become another simulation. Integrate stable identities, schedules, memories and threads into the reducer, hash and versioned save.
- **Clock pacing:** long walks are expensive under the existing time contract. Place daily destinations deliberately and budget travel before writing dialogue promises.
- **Collision and navigation:** reserve usable space before dressing it. Colour buckets should not remain the authority for whether an object is solid.
- **Performance:** instance static architecture; update only visible character presentation frequently; suspend ornament timers when hidden. Measure against the project’s block allowances—7 ms GPU, 6 ms game thread and 1,250 visible draw calls on the proposed desktop target—rather than assuming twelve simple NPCs are automatically cheap.
- **Scope tracking:** keep every house on the completion inventory. Reusable shells accelerate delivery; permanent façade-only substitutes do not fulfil the request.
- **Fairness and tone:** give care, ordinary work and non-supernatural lives equal visual attention. Preserve the existing cast, review new relationships and names, and avoid turning every resident into a quest dispenser.
- **Verification:** the current walking controller and scale edits are a stronger starting point, but source inspection cannot establish movement feel, camera clearance, readable faces or a convincing inhabited street. Those remain explicit playtest gates.