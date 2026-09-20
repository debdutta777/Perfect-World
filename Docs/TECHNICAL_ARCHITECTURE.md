# Technical architecture — Perfect World: Awakening

Status: target architecture plus a deliberately small implemented creation core. Read BUILD_GUIDE.md for actual build evidence. This document does not claim the simulation, combat, school, rendering pipeline or platform ports already exist.

## Decision and constraints

Use the installed Unreal Engine 5.7 release and C++ for authoritative rules, state transitions and budget validation. Use Blueprints for presentation, authored events and rapid tuning after their underlying interfaces stabilize. The portable creation rules can run in a small standalone test executable; Unreal exposes the same rules to UI. Do not duplicate budget math in widgets.

Windows is the initial development and test host. Android is an early performance acceptance target because the supplied concept is mobile-first. iOS and consoles are future feasibility milestones requiring platform tooling, device testing and distribution access. C++ portability helps reuse rules but does not make a game automatically cross-platform. No multiplayer, cross-save service, account system or live-service dependency is included in the first slice.

## Layers and ownership

| Layer | Owns | Must not own |
|---|---|---|
| Portable domain core | Creation catalog, validation, point ledger, start rules | Actors, assets, UI strings, platform APIs |
| Unreal bridge | Blueprint-callable API, conversion to engine types | A second implementation of the ledger |
| Life simulation, planned | Calendar, needs, school attendance, scheduled activity completion | Frame-by-frame cosmetic work |
| Narrative, planned | Conditions, journal, event consequences, story flags | Direct uncontrolled mutation of every subsystem |
| Combat, planned | Encounters, damage, abilities, cooldowns, rewards | Real-world career progression rules |
| World presentation, planned | 3D world space, navigation, weather visuals, camera, and the character-display interface (see below) | Persistent truth about offscreen NPC locations; gameplay rules that key off how a character is currently drawn |
| UI, planned | Creation steps, phone, school planner, accessibility | File access, balance authority, paid entitlements |
| Persistence, planned | Versioned snapshot, migration, atomic write, recovery | Serializing raw actor pointers |

First keep these as folders and clean interfaces in one game module. Extract plugins only when a boundary has independent content, tests and ownership. Splitting every feature into a plugin now increases build and asset-management overhead without proving a benefit.

## Presentation layer: 2D now, 3D later (design decision, 2026-09-20)

This extends, rather than replaces, the target architecture: gameplay, simulation, and world state stay in 3D world space and stay authoritative, exactly as before. What changes is that **how a character is displayed is a separate, replaceable layer**, not an assumption baked into the simulation. Today that layer composes a 2D anime-styled sprite/paper representation (driven by the appearance parameter set described in CHARACTER_CREATION.md) positioned and oriented in the 3D world; later it can be a fully skeletal 3D mesh rig driven by the same parameter set, with no change to anything below the presentation layer.

The rule this buys, stated plainly so it cannot be silently violated: **nothing in the rules or simulation may depend on whether a character is currently drawn as a sprite or a mesh.** Concretely:

- Position, collision, navigation, interaction, and simulation state live in 3D world space regardless of display mode.
- A `ICharacterDisplay`-style interface (name indicative, not yet implemented) sits between world presentation and the actual drawn representation. Simulation and gameplay code call through this interface (e.g. "play this animation state," "face this direction") and never touch a sprite component or a skeletal mesh component directly.
- The appearance parameter set (CHARACTER_CREATION.md) is the only appearance data gameplay code is allowed to read. It is display-mode-neutral by construction.
- Save data stores the appearance parameter set and world-space transforms, never a sprite asset reference or a mesh asset reference as the source of truth.

What this buys later: moving to full 3D character display becomes a presentation-layer swap — implement the interface against skeletal meshes and retire the sprite implementation — not a rewrite of simulation, saves, or rules. It also means the two can coexist during a transition (some characters shown as sprites, others as meshes) without branching gameplay logic, since gameplay never queries which one is active.

This decision does not change the "World presentation, planned" row above; it specifies what that row's ownership boundary must look like once built.

## Creation transaction

UI requests a rank change by stable trait ID. Core checks ID, rank domain, budget and premium-trait cap against a candidate copy. Only a valid candidate replaces the draft. UI queries spent and remaining from total cumulative costs. Refunds are a consequence of a lower rank, never a separate grant command. Free appearance/identity selections stay outside this economic ledger. Name length, local text input and cosmetic asset validity require UI/serialization validation separately. The current Slate prototype accepts a free given name and fixes its opening city label to Kolkata; it does not implement the full avatar editor.

The review step displays final age16, school year, start location, background facts, selected advantages and remaining points. The first committed state becomes an origin record. A subsequent New Game creates a fresh origin. A future ordinary save load must not repeat grants or reset already-played history. Do not serialize a user-controlled spent total as authoritative.

## Time, school and aging (planned)

Use a monotonically increasing integer game-minute count and explicit calendar conversion. Simulation actions reserve duration and apply effects once on completion. Pausing a menu stops simulation; resuming never charges the activity twice. Exploration can advance time continuously, while classes and sleep advance a known interval through the same event queue. Gate encounters use an explicitly configured outside-time rate, not an unrelated clock.

Separate biological age, school progression and playtime. Do not age the player from16 to18 within a week of missions. Birthdays change age; assessments and attendance change academic state; chapter transitions can offer an explained time skip only after unresolved commitments are summarized. Full school years are a later production decision; the first slice covers a small authored span at age16.

A scheduled event has stable ID, due minute, priority, prerequisite, payload and completion token. On a time jump, process due events in deterministic order with an event cap and carry-over. Random opportunities use persisted seeds and cooldowns so reloading does not reroll an arbitrary new week. Important events produce explainable journal entries. Use game-defined permission gates for youth training; do not describe fictional Gate laws as real Indian law.

## NPC and city scaling (planned)

Only near-camera NPCs need a full display representation (2D composed sprite today, skeletal mesh later — see the presentation-layer decision above), animation/state, path following and interaction components. Other residents have schedule records, coarse location, relationship state and story flags. Upgrade a resident to an Actor when relevant and reconcile back to its record when released. A named character keeps the same stable identity through both representations, and through a future swap from sprite to mesh display.

Prototype a modest district with level instances/interior loads and explicit travel transitions. Gate maps are separate encounter spaces. Evaluate World Partition when actual district size and streaming measurements justify it, rather than using a national continuous map as the starting architecture. India expands by authored cities, regional event packs and a national travel graph. Trains or flights can be time-and-money transitions, not kilometers of empty terrain.

For the slice, prefer simple Behavior Trees or StateTree only after profiling and a single chosen behavior-authoring workflow. Mass Entity is an investigation for crowd scale, not a mandatory foundation. A few dozen well-authored people matter more than thousands of contextless agents.

## Combat and abilities (planned)

Start with one weapon family, basic attack, evade and one awakened ability. Evaluate Unreal's Gameplay Ability System in a small combat spike before adopting it across every life stat. If adopted, use attributes for combat, Gameplay Tags for status and eligibility, Gameplay Effects for modifications and gameplay cues for visual/audio effects. School attendance and family budgets remain domain systems rather than becoming combat attributes.

Awakening is a rule-based interpretation of player history. It does not call an LLM during play. Record bounded, meaningful events such as completed practical work, rescues and movement training. Normalize scores and apply authored thresholds; present why an affinity emerged. Allow later retraining and alternative unlocks so a first-hour choice does not invalidate a hundred-hour save. Combination abilities are authored recipes with input tags and explicit prerequisites, not combinatorial generated abilities with untested balance.

## Data and asset policy (planned)

Stable IDs such as Trait.family_resources, NPC.meera and Quest.first_alarm must survive renaming. Author text as localized text, not raw concatenated strings. Separate displayed names from identifiers. Money uses integer smallest chosen game currency units; points and rank costs are integers. Effects specify units and bounds. Use Primary Data Assets/Data Tables where designers need inspector editing, with a validator for missing references, duplicate IDs and impossible requirements. The portable core's small creation catalog is currently compiled data; externalizing it requires schema validation and parity tests.

Avoid hard references from the player class to all national content. Load location bundles and encounter assets asynchronously at transition boundaries. Content naming: CH_ character, ENV_ environment, UI_ interface, FX_ effect, DA_ authored data, MAP_ level. Establish the naming convention before bulk imports. Generated concept PNGs are reference art and are not meshes, textures ready for UV mapping, animation, or shipping UI assets.

## Save system acceptance criteria (planned)

Save includes schema version, build identifier, origin and draft distinction, game minute, deterministic seeds, persistent entity IDs, quest tokens, inventory instances and relationship state. Write a temporary file, validate, then atomically replace the primary slot while keeping one known-good backup. Save at explicit safe checkpoints rather than mid-transaction. Test corrupted input, absent optional fields, migration from the immediately previous version and interruptions during write. No promises of cloud compatibility until platform identity and conflict rules are designed.

## Rendering and cross-platform plan

Art target: stylized low-poly 3D environments and characters, fixed isometric camera, painted portraits, restrained material complexity. Prototype baked/precomputed light for the smallest mobile tier with controllable time-of-day variants; use a simplified dynamic light only where tested. Day/night art may require separate lighting states or a hybrid solution, so do not silently promise both fully baked lighting and arbitrary dynamic sun positions.

Epic documents Mobile Forward as a suitable path for precomputed lighting; Mobile Deferred has a different feature/performance trade-off. Select through target-device measurements, not a blanket claim that one is always faster. Do not require desktop ray tracing, Lumen, dense Nanite scenery or high-end volumetrics for the baseline. See [Epic's UE5.7 mobile rendering guidance](https://dev.epicgames.com/documentation/en-us/unreal-engine/mobile-rendering-and-shading-modes-for-unreal-engine?application_version=5.7).

Initial experimental budgets (acceptance targets, not measurements): Windows1080p60 with p95 frame time at or below16.7ms on a nominated midrange test PC; Android sustained30fps with p95 at or below33.3ms during a20-minute route including school crowd, rain and one Gate. Record exact device, resolution, quality setting, thermals, memory and build. Do not publish a minimum supported device before the packaged build is profiled. Reserve frame time for UI, simulation and streaming, not just rendering. Test memory over repeated transitions and suspend/resume, not only a fresh launch.

Unreal Enhanced Input should map the same actions to mouse/keyboard, controller and touch. UI requires navigation focus, scalable text, remapping, subtitle options, safe areas, pause behavior and alternate cues beyond color. Phone overlays must leave touch controls reachable. Add Android packaging and one real device in the first technical milestone; iOS needs a separate Apple tooling path. Console certification is a later separate project milestone.

## Build and quality gates

1. Standalone C++ rules compile with warnings and run real rejection/refund/property-style tests.
2. Unreal Header Tool and Editor target compile using the pinned installed engine and supported compiler.
3. A blank-map Play In Editor smoke test can retrieve catalog, edit the draft and reject overspending.
4. A cooked Windows build starts; no editor-only content dependency enters runtime.
5. An Android development package passes launch, touch, pause/resume and measured scene budgets.
6. Save migration and authored content validation run before content volume grows.

Each milestone must report which gates passed. A passing portable test does not verify UHT, a cooked build or device performance. Epic's [UE5.7 Visual Studio setup page](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.7) identifies supported tools; check local installed versions before attempting broad SDK changes.
