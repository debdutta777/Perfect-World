# Perfect World Player Guide

A practical guide for testing and playing development builds in the Nabapara worktree.

---

## 1. Starting a Life

Every new life begins in Class XI at Nabapara Vidyalaya in Kolkata, West Bengal, at exactly 16 years of age.

### Creation Budget and Traits
- You receive a fixed budget of **1,000 creation points**.
- Point purchases are strictly bounded: all 1,000 points must be spent before life begins.
- There are 20 traits arranged across seven categories. Each trait can be set from Rank 0 to Rank 4.
- Ranks cost points on an escalating scale:
  - Rank 0: 0 points
  - Rank 1: 25 points
  - Rank 2: 60 points
  - Rank 3: 110 points
  - Rank 4: 180 points
- **Rank 4 cap:** At most four traits may be raised to Rank 4.
- Identity (Given Name, Family Name, Sex, Date of Birth) and visual appearance cost 0 points. Supernatural classes cannot be purchased.

### Trait Categories
| Category | Traits |
|---|---|
| Appearance & Presentation | Physical Presence, Personal Presentation |
| Family Resources & Support | Household Resources, Guardian Support, Housing Stability |
| Physical | Vitality, Strength, Agility |
| Cognition | Analytical Reasoning, Learning Discipline, Perception |
| Talents | Technical Aptitude, Creative Aptitude, Athletic Aptitude |
| Social | Empathy, Confidence, Community Connections |
| Prior Experiences | Academic Foundation, Practical Experience, Adversity Resilience |

### Household Templates and Dwellings
- Five household structures are available:
  - **Parent household:** Compact family flat.
  - **Extended family:** Row house.
  - **Grandparent household:** Older cooperative flat.
  - **Adult sibling household:** Rented flat.
  - **Appointed guardian:** Staff quarters with private sleeping space.
- **Household Resources Rank 0** is not merely a poorer flat; it changes your upbringing to **institutional care** (Children's home). Ranks 1 to 4 correspond to Modest, Steady, Comfortable, and Ample households.

### Verified Presets
If you do not wish to allocate 20 individual traits manually, five balanced presets are built in:
1. **Balanced Scholar:** Broad academic and social foundation (0 Rank 4 traits).
2. **Resourceful Maker:** Focus on Technical Aptitude and Practical Experience (Rank 4 Practical Experience).
3. **Athletic Organizer:** High physical attributes and Community Connections (Rank 4 Athletic Aptitude).
4. **Creative Connector:** Creative Aptitude with strong social skills (0 Rank 4 traits).
5. **Independent Generalist:** Institutional care upbringing (Household Resources Rank 0, 0 Rank 4 traits).

### School Streams
You must select one of three Higher Secondary streams for Class XI:
- **Science:** Focuses on Mathematics, Physics, Chemistry, and laboratory practicals.
- **Commerce:** Focuses on Accountancy, Business Studies, Economics, and Mathematics.
- **Humanities:** Focuses on History, Geography, Political Science, and Sociology.
All three streams share compulsory English and Bengali periods.

<!-- Sources: Source/PerfectWorldCore/Private/CharacterCreation.cpp, Source/PerfectWorldCore/Private/CreationPresets.cpp, Source/PerfectWorldCore/Private/SaveSlotSummary.cpp, Docs/GAME_PLAN.md -->

---

## 2. Controls

The table below lists the default key bindings for keyboard and gamepad.

### Control Table
| Action | Action ID | Group | Keyboard Default | Gamepad Default | Rebindable |
|---|---|---|---|---|---|
| Move Forward | `move_forward` | Movement | W | Left Stick (Axis) | Keyboard: Yes, Gamepad: No |
| Move Back | `move_back` | Movement | S | Left Stick (Axis) | Keyboard: Yes, Gamepad: No |
| Move Left | `move_left` | Movement | A | Left Stick (Axis) | Keyboard: Yes, Gamepad: No |
| Move Right | `move_right` | Movement | D | Left Stick (Axis) | Keyboard: Yes, Gamepad: No |
| Run | `run` | Movement | Left Shift | Left Stick Click (L3) | Yes |
| Jump | `jump` | Movement | Space Bar | A (Bottom Face Button) | Yes |
| Interact | `interact` | Actions | E | X (Left Face Button) | Yes |
| Cycle Target | `cycle_target` | Actions | Tab | RB (Right Shoulder) | Yes |
| Toggle Camera | `toggle_camera` | Camera | V | Y (Top Face Button) | Yes |
| Zoom In | `zoom_in` | Camera | Mouse Scroll Up | D-Pad Up | Yes |
| Zoom Out | `zoom_out` | Camera | Mouse Scroll Down | LB (Left Shoulder) | Yes |
| Map | `map` | Interface | M | D-Pad Left | Yes |
| Quests | `quests` | Interface | J | D-Pad Right | Yes |
| Pause Menu | `pause` | Interface | Escape | Menu / Start (Special Right) | No (Reserved) |
| Inventory | `inventory` | Interface | I | D-Pad Down | Yes |

### Rebinding and Settings
- **Where to remap:** Open the Pause Menu (Escape / Gamepad Menu), select **Settings**, and choose the **Controls** tab.
- **Gamepad Zoom Out:** Zoom-out was moved from D-Pad Down to LB in build v0.7.0 so that D-Pad Down could open the Inventory.
- **Reserved Keys:** Escape and Gamepad Special Right are reserved for the Pause Menu and cannot be rebound.
- **Mouse Sensitivity:** Adjusted under the **General** tab in Settings (range 0.2 to 3.0, default 1.0).

<!-- Sources: Source/PerfectWorld/Private/PerfectWorldKeyBindings.cpp, Source/PerfectWorld/Private/UI/SPWSettingsPanel.cpp, Source/PerfectWorld/Private/UI/SPWControlsSection.cpp, Source/PerfectWorld/Private/UI/SPWPauseMenu.cpp -->

---

## 3. Your First Day: A Walkthrough from Home to School

This walkthrough takes you from your bed to the classroom seat on Day 1.

### Step 1: Leaving the Flat
1. You wake up in your bedroom inside the family flat on Nabapara Lane.
2. Walk out of the bedroom into the living room. Pass the dining table and chairs (the kitchen and bedroom doors open inward and do not obstruct the walkway).
3. Approach the flat's outer entrance door. Press **E** (or Gamepad **X**) to open the door and step out onto the pavement of Nabapara Lane.

### Step 2: Visiting Shanti Chai
1. Head along the lane towards the tea shop, Shanti Chai.
2. Walk through the doorway to the counter.
3. As you approach the counter, the **Buy** prompt appears by default (priority 25).
4. If you wish to speak to keeper Farida Qadri, press **Tab** (or Gamepad **RB**) to cycle the prompt to **Talk** (priority 20). Pressing Tab again switches back to Buy.
5. With Buy selected, press **E** (or Gamepad **X**) to open the shop panel.
6. Select **Biscuits** (₹10.00). Confirm the purchase.
7. Your wallet balance beside the clock drops by ₹10.00 (e.g. from ₹150.00 to ₹140.00 in a modest home), and a receipt is displayed.

### Step 3: Checking Inventory
1. Press **I** (or Gamepad **D-Pad Down**, or select Inventory from the Pause Menu).
2. The Inventory panel lists your purchased biscuits.
3. Select the biscuits and choose **Consume**. Your hunger is relieved immediately, spending zero game minutes.
4. Close the inventory to return to the lane.

### Step 4: Arriving at Nabapara Vidyalaya
1. Walk south down Nabapara Lane to the school boundary wall and south gate (located at coordinates X: 6500, Y: 4850).
2. The school gates unlock at **09:30**.
3. Outside the gate, interact with the timetable board (**E** / **X**) to read the posted daily periods and bells.
4. Step up to the gate attendance register between **09:40** and **09:58** (the on-time arrival window before assembly at 10:00).
5. Interact with the register (**E** / **X**) to "Sign in at the gate". The screen will confirm your attendance as **on time**.

### Step 5: Taking Your Seat in Class
1. Pass through the south gate opening into the school forecourt.
2. Walk across the courtyard towards the teaching building on the west.
3. Follow the step-free verandah corridor up along the office bay. Walk down the verandah walkway to avoid the support pillars standing at Y: 6175.
4. Enter the ground-floor Class XI room through the open doorway.
5. Walk down the aisle to the student desk.
6. When prompted with **Take your seat**, press **E** (or Gamepad **X**).
7. The School-Day timetable panel opens, and the world clock holds while the panel is visible.

<!-- Sources: Source/PerfectWorld/Private/PerfectWorldAcceptanceCommerce.cpp, Source/PerfectWorld/Private/PerfectWorldAcceptanceSchool.cpp, Source/PerfectWorld/Private/PerfectWorldSchoolCampus.cpp, Source/PerfectWorld/Private/PerfectWorldSchoolGate.cpp, release/notes_v0.7.0-alpha.md, release/notes_v0.7.1-alpha.md, release/notes_v0.8.0-alpha.md, release/notes_v0.8.1-alpha.md -->

---

## 4. Shops

Five retail counters are staffed and operational in Nabapara Lane. Prices are tracked in integer paise (100 paise = ₹1.00).

### Retail Shops and Vendors
| Shop Name | Keeper(s) | Counter Anchor | Opening Hours | Goods Sold | Price | Unit & Effect |
|---|---|---|---|---|---|---|
| **Shanti Chai** | Farida Qadri | `anchor.shanti_chai.counter` | 06:00–11:30 & 15:30–19:00 (Daily) | Tea<br>Biscuits | ₹25.00 (2,500p)<br>₹10.00 (1,000p) | Cup: +2,000 Energy (60 min cooldown)<br>Pair: +1,500 Hunger Relief |
| **Stationery shop** | Anindita Sen | `anchor.stationery.counter` | 08:00–13:00 & 15:00–19:30 (Daily) | Pen<br>Notebook<br>Geometry box | ₹20.00 (2,000p)<br>₹60.00 (6,000p)<br>₹220.00 (22,000p) | Pen: stack 10, study supply<br>Notebook: stack 10, study supply<br>Box: stack 10, study supply |
| **Sweet shop** | Bimal Modak & Tithi Ghoshal | `anchor.nabapara.sweet_shop_counter` | 07:00–13:30 & 15:30–21:00 (Daily) | Rasgulla<br>Sandesh | ₹30.00 (3,000p)<br>₹35.00 (3,500p) | Piece: +1,000 Hunger Relief<br>Piece: +1,000 Hunger Relief |
| **Jhalmuri & phuchka vendor** | Koushik Mondal | `anchor.nabapara.vendor_counter` | 15:00–18:00 & 18:20–21:00 (Daily) | Jhalmuri<br>Phuchka | ₹40.00 (4,000p)<br>₹50.00 (5,000p) | Portion: +3,500 Hunger Relief<br>Plate of six: +3,000 Hunger Relief |
| **Pharmacy** | Aparna Lahiri | `anchor.nabapara.pharmacy_counter` | 09:00–14:00, 14:30–17:00 & 17:15–23:00 (Daily) | Plasters<br>ORS | ₹40.00 (4,000p)<br>₹30.00 (3,000p) | Pack of five: carryable supply<br>Sachet: carryable supply |

> [!NOTE]
> Other neighbourhood establishments—such as Mahua Saha's hardware store and Debashis Pal's repair workshop—feature talkable proprietors and routines, but have no retail commerce catalogue registered in this build.

<!-- Sources: Data/Content/commerce_shop/shop.shanti_chai.json, Data/Content/commerce_shop/shop.stationery.json, Data/Content/commerce_shop/shop.sweet_shop.json, Data/Content/commerce_shop/shop.vendor.json, Data/Content/commerce_shop/shop.pharmacy.json, Data/Content/commerce_item/item.tea.json, Data/Content/commerce_item/item.biscuits.json, Data/Content/commerce_item/item.pen.json, Data/Content/commerce_item/item.notebook.json, Data/Content/commerce_item/item.geometry_box.json, Data/Content/commerce_item/item.rasgulla.json, Data/Content/commerce_item/item.sandesh.json, Data/Content/commerce_item/item.jhalmuri.json, Data/Content/commerce_item/item.phuchka.json, Data/Content/commerce_item/item.plasters.json, Data/Content/commerce_item/item.ors.json -->

---

## 5. School Day and Lessons

Nabapara Vidyalaya operates Monday to Friday under a unified bell schedule.

### Daily Schedule
- **09:30 (570 min):** Gates open.
- **10:00–10:15 (600–615 min):** First bell rings; morning assembly in the school court (`nhss.court`).
- **10:20–11:05 (620–665 min):** Period 0 (45 minutes).
- **11:10–11:55 (670–715 min):** Period 1 (45 minutes).
- **12:00–12:45 (720–765 min):** Period 2 (45 minutes).
- **12:50–13:20 (770–800 min):** Tiffin break (30 minutes) in the school court.
- **13:25–14:10 (805–850 min):** Period 3 (45 minutes).
- **14:15–15:00 (855–900 min):** Period 4 (45 minutes).
- **15:05–15:50 (905–950 min):** Period 5 (45 minutes).
- **15:50 (950 min):** Final dismissal bell.

### Timetables by Stream
| Period | Science | Commerce | Humanities |
|---|---|---|---|
| **Period 0** | Mathematics (Model) | Accountancy (Investigate) | History (Explain) |
| **Period 1** | Physics (Model) | Business Studies (Coordinate) | Geography (Model) |
| **Period 2** | Chemistry (Coordinate) | Economics (Explain) | Political Science (Coordinate) |
| **Period 3** | English (Explain) | English (Explain) | English (Explain) |
| **Period 4** | Physics Lab (Investigate)* | Mathematics (Model) | Sociology (Coordinate) |
| **Period 5** | Bengali (Explain) | Bengali (Explain) | Bengali (Explain) |

*\* Note: Science Period 4 takes place in the Science Lab (`nhss.science_lab`). All other periods take place in the stream's respective home room.*

### Lesson Structures
Lessons are delivered through four interactive exercise screens:
1. **Model (`problem_family.xi_stock_and_flow`):** Solve stock and flow rate problems. Test readings, understand variable links, and explain calculations.
2. **Explain (`explain_family.xi_claim_evidence_limit`):** Build a defensible case through three sequential beats: choose a claim, attach supporting evidence, and acknowledge boundaries or limitations.
3. **Investigate (`investigate_family.xi_fault_finding`):** Diagnose broken equipment or mismatched records. Conduct low-cost tests, inspect parts, swap components, and isolate faults.
4. **Coordinate (`coordinate_family.xi_group_work`):** Manage group tasks. Assign roles, balance team capacities, resolve conflicting constraints, and submit reports cleanly.

### Lesson Modes and Clock Behaviour
- **Foreground:** Directly play through the interactive lesson screen. Choices spend in-game time. Leaving early charges only the minutes used; finishing records completion.
- **Attend:** Passively sit through the lesson at the desk.
- **Routine:** Background execution (not available for the first period).
- **World Clock Hold:** While a lesson screen or day panel is open, the external world clock pauses until the activity concludes or is closed.

<!-- Sources: Data/Content/bell_schedule/bell_schedule.nabapara_xi_science.json, Data/Content/bell_schedule/bell_schedule.nabapara_xi_commerce.json, Data/Content/bell_schedule/bell_schedule.nabapara_xi_humanities.json, Data/Content/school_lesson/p2.science.json, Data/Content/school_lesson/p2.commerce.json, Data/Content/school_lesson/p2.humanities.json, Data/Content/school_lesson/p2.school.json, Source/PerfectWorld/Private/PerfectWorldAcceptanceSchool.cpp, Source/PerfectWorldCore/Private/SchoolLessonContent.cpp -->

---

## 6. Study Desk and Week Planner

Academic work continues at home through your desk and planner.

### Study Desk
- Interact with the desk in your bedroom or living quarters (**Study at the desk**).
- **Academic Record:** Displays past lesson attendance, completion marks, and examination records.
- **Practice Offers:** Displays permitted self-study activities, including `activity.study_at_home` ("Study at home"), charging time through the standard day loop.
- **Assessment Papers:** Allows taking 90-minute structured tests (18 five-minute units) divided into question groups. Papers can be started, answered, submitted, or cancelled.
- **Homework Status:** The study desk interface is fully functional, but automated issuance of daily homework assignments via calendar triggers is scheduled for Phase 3 (P3-04); the homework tray may therefore appear empty.

### Week Planner
- Open the Pause Menu and select **Week planner** (located directly below Inventory).
- Previews the upcoming seven-day school week based on your stream's active calendar and bell schedule.
- Allows setting **standing orders** for periods.
- **Resolve Week:** Runs automated resolution across the school week, stopping immediately whenever player intervention is required.

<!-- Sources: Source/PerfectWorld/Private/PerfectWorldSchoolLiveWork.cpp, Source/PerfectWorld/Private/UI/SPWPauseMenu.cpp, Source/PerfectWorldCore/Private/SchoolWork.cpp, Source/PerfectWorldCore/Private/SchoolWeek.cpp, release/notes_v0.8.0-alpha.md, release/notes_v0.8.1-alpha.md -->

---

## 7. Awakening Day and the Ten Days

The supernatural element of Perfect World is introduced through a formal, mandatory school event.

### Awakening Day: 23 January 2043
- On 23 January 2043, every Class XI student attends the school tutorial dungeon.
- **Compulsory Attendance:** You cannot refuse to enter the dungeon; it is a national requirement.
- At your scheduled appointment time (`AppointmentAt`), approach the campus terminal pad and interact to enter.

### Tutorial Dungeon
- The tutorial consists of three non-combat situations:
  1. Assist a distressed classmate.
  2. Shield the student group.
  3. Arrange supervised handover.
- **Safe Withdrawal:** You may withdraw safely at any point. A make-up session is permitted before the final deadline.
- Completing the supervised run grants your Awakening profile atomically and charges exactly 60 minutes.

### Awakening Result Panel
- Upon completion, the result screen opens automatically.
- Displays your affinity across the **Five Tattvas**:
  - Agni (Fire)
  - Vayu (Air)
  - Jala (Water)
  - Prithvi (Earth)
  - Akasha (Aether / Void)
- Displays ratings across the six power capability dimensions.

### The Ten Days and Countdown
- At exactly **18:00** on 23 January 2043, the countdown panel unlocks (it remains locked prior to 18:00).
- The countdown monitors the ten days leading up to the national examination on **2 February 2043 at 06:00**.
- During these ten days, ordinary school classes are suspended (`phase2.a1.closure.break.ten_days`).

### Complete Freedom (The Supernatural is Optional)
- You are under no obligation to pursue supernatural powers.
- The countdown screen provides an explicit **Keep ordinary day** action, which costs 0 minutes.
- A player who ignores combat and supernatural cultivation retains a full, complete game experience.

### What Exists vs What is Not Connected Yet
| Feature | Code Status | Notes |
|---|---|---|
| Tutorial Dungeon | Implemented | 3 non-combat situations and withdrawal verified in tests. |
| Awakening Persistence | Implemented | Saved in Schema 12 (hash 9) under `pw.awakening/1`. |
| Result & Countdown Panels | Implemented | UI displays Tattvas, capabilities, and countdown clock. |
| Lesson-to-Awakening Growth | Not Connected | In v0.8.1, classroom lessons do not yet dynamically increment latent Awakening attributes. |
| Virtual Tower Examination | Not Connected | Playable tower dungeon runs belong to post-v0.9.0 releases; current code stops at 2 February 2043, 06:00. |
| Countdown Contacts | Not Connected | Contact lists remain deliberately empty; no mock contacts are fabricated. |

<!-- Sources: Docs/PHASE3_PLAN_2026-11-09.md, Source/PerfectWorld/Private/PerfectWorldAcceptanceAwakening.cpp, Source/PerfectWorldCore/Private/TutorialDungeon.cpp, Source/PerfectWorldCore/Private/WorldCountdown.cpp, Source/PerfectWorldCore/Private/WorldSaveAwakening.cpp, release/notes_v0.8.0-alpha.md, release/notes_v0.8.1-alpha.md -->

---

## 8. Saving and Continue

The save architecture uses atomic state files validated by SHA-256 state hashes.

### Save System Overview
- **Save Schemas:**
  - **Schema 10 (Hash 7):** Added commerce, wallet, and inventory (`pw.commerce/1`).
  - **Schema 11 (Hash 8):** Added school sessions, completed lessons, papers, and standing orders (`pw.school/1`).
  - **Schema 12 (Hash 9):** Added Awakening grant, tutorial dungeon progress, and countdown state (`pw.awakening/1`).
- **How to Save:** Open the Pause Menu (Escape / Gamepad Special Right) and click **Save**.
  - A successful save displays: `"Saved."` or `"Saved X times."`
  - A failed save displays: `"Not saved. <Reason>"`.

### Continue and Migration
- Select **Continue** from the Title Screen to resume the latest valid save slot.
- Older saves from earlier schemas (v9, v10, v11) are automatically migrated to the current schema upon loading.
- Upgrading a pre-commerce save automatically grants your household's starting money once.
- Save slots track and display:
  - Character Full Name
  - Current Age
  - In-game Date and Time
  - Zone Name (e.g. `"Nabapara Lane"` or `"Compact family flat"`)
  - Home City (`"Kolkata"`)

<!-- Sources: Source/PerfectWorldCore/Private/WorldSave.cpp, Source/PerfectWorldCore/Private/SaveSlotSummary.cpp, Source/PerfectWorldCore/Private/WorldSaveCommerce.cpp, Source/PerfectWorldCore/Private/WorldSaveSchool.cpp, Source/PerfectWorldCore/Private/WorldSaveAwakening.cpp, Source/PerfectWorld/Private/UI/SPWPauseMenu.cpp -->

---

## 9. Player Test Checklist

Use this 18-point checklist to test gameplay and interface systems in your build:

1. **Point Budget Enforcement:** In Character Creation, try starting life with 950 or 1,050 points allocated. Verify the game blocks you with: `"Allocate all 1000 creation points before beginning life."`
2. **Rank 4 Attribute Cap:** In Character Creation, try assigning five traits to Rank 4. Verify the game displays: `"At most four traits may be raised to rank 4."`
3. **Preset Verification:** Select the **Balanced Scholar** preset. Verify that the total points equal exactly 1,000 and zero traits are at Rank 4.
4. **Institutional Care Upbringing:** Select the **Independent Generalist** preset (Household Resources Rank 0). Verify your home is designated as the **Children's home**.
5. **Camera Perspective Toggle:** Press **V** (or Gamepad **Y**). Verify the camera switches smoothly between first-person and third-person view.
6. **Camera Zooming:** Scroll the **Mouse Wheel** (or use Gamepad **D-Pad Up** / **LB**). Verify that camera distance zooms in and out.
7. **Flat Navigation:** Walk from the bedroom, through the doorway, past the kitchen, and up to the dining chair. Verify that open doors swing into rooms and do not block the path.
8. **Street Transition:** Approach the flat door, press **E** (or Gamepad **X**), and step out onto Nabapara Lane.
9. **Counter Interaction Priority:** Walk to Shanti Chai at 09:00. Verify the prompt displays **Buy** (priority 25) rather than Talk.
10. **Target Cycling:** Press **Tab** (or Gamepad **RB**) at the counter. Verify the prompt toggles to **Talk** with Farida Qadri, and press Tab again to return to Buy.
11. **Purchasing Biscuits:** Open the Buy panel, select Biscuits (₹10.00), and confirm. Verify your wallet decreases by ₹10.00 and a receipt is shown.
12. **Inventory Consumption:** Press **I** (or Gamepad **D-Pad Down**), highlight Biscuits, and click **Consume**. Verify the item is removed, hunger is reduced, and the world clock does not advance.
13. **Closed Shop Refusal:** Approach Shanti Chai at 13:00 (during midday closure). Attempt to buy and verify the notice: `"Shanti Chai is closed right now. Check the posted hours."`
14. **School Timetable Board:** Walk south to Nabapara Vidyalaya gate. Press **E** on the board. Verify the posted bell times and daily timetable appear.
15. **On-Time Sign In:** Between 09:40 and 09:58, press **E** on the attendance register. Verify the screen displays an `"on time"` attendance confirmation.
16. **Classroom Seat Interaction:** Walk down the verandah corridor into the Class XI room. Approach the student desk and press **E** to "Take your seat". Verify the School-Day panel opens and the world clock pauses.
17. **Foreground Lesson:** Select an available period and choose **Foreground**. Verify the lesson screen opens (e.g. Model, Explain, Investigate, or Coordinate). Make valid choices and click Record; verify the period completes and the elapsed time is charged.
18. **Week Planner Preview:** Press **Escape**, click **Week planner** in the Pause Menu, and verify that the 7-day school timetable plan is displayed.
19. **Saving and Continuing:** Click **Save** in the Pause Menu. Confirm the `"Saved."` notification. Quit to the Title Screen, select **Continue**, and verify you resume with your exact wallet balance, inventory, and clock time intact.

<!-- Sources: release/notes_v0.7.0-alpha.md, release/notes_v0.7.1-alpha.md, release/notes_v0.8.0-alpha.md, release/notes_v0.8.1-alpha.md, Source/PerfectWorld/Private/PerfectWorldAcceptanceCommerce.cpp, Source/PerfectWorld/Private/PerfectWorldAcceptanceSchool.cpp, Source/PerfectWorld/Private/UI/SPWPauseMenu.cpp -->

---

## 10. Known Gaps

The following issues and boundaries are present in the current build:

1. **Automated School Pathing Issue (`school_lesson_round9`):**
   In automated testing on v0.8.1, the bot walked through the school gate and signed in, but halted 3.3 metres short of the classroom seat at coordinates (7202, 6214) due to collision with verandah pillars. Human players can navigate around the pillars manually, but automated waypoint pathing remains under investigation.
2. **Homework Brief Issuance:**
   The study desk mechanics for homework and assessments are fully built in the core engine. However, scheduled issuance of briefs via school calendar triggers arrives in Phase 3 (P3-04); the desk homework queue may initially be empty.
3. **Playable Virtual Tower Examination:**
   Full interactive runs inside the virtual tower examination are planned for releases following v0.9.0. Current gameplay concludes at the examination morning boundary (2 February 2043, 06:00).
4. **Dynamic Lesson-to-Awakening Progression:**
   In build v0.8.1, completing classroom lessons does not yet dynamically increment latent Awakening attributes. The lesson credit adapter (P3-02) connects this in Phase 3.
5. **Medical Supply Usage:**
   Plasters and ORS purchased from the pharmacy are tagged as `unsupported_supply`. They can be carried in inventory, but specific injury conditions and treatment mechanics are not yet implemented.
6. **Tutorial Dungeon Non-Combat Scope:**
   The Awakening tutorial sequence focuses exclusively on non-combat teamwork, shielding, and safe withdrawal. Supernatural combat mechanics are not featured in the school tutorial.
7. **Voxel Block Art Style:**
   In accordance with the development plan, all visual assets throughout the world (buildings, furniture, props) remain voxel block geometry until Milestone M9. Gameplay systems interface purely with asset IDs.

<!-- Sources: release/notes_v0.7.0-alpha.md, release/notes_v0.8.0-alpha.md, release/notes_v0.8.1-alpha.md, Docs/PHASE3_PLAN_2026-11-09.md, Docs/GAME_PLAN.md -->
