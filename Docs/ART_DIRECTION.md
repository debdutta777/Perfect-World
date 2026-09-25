# Art direction and production brief

**Notice, 2026-09-23.** This document now describes the **final-art direction** — the stylised 3D look the game moves to at milestone M9, after the game is complete. During development, every visual (environments and characters) is built from voxel blocks — cuboids with pixel textures, a look rather than an engine — per `GAME_PLAN.md` §3; the block style guide, asset IDs and the swap contract are in `Docs/ASSET_PIPELINE.md`. The Kolkata setting palette, material grammar and culture rules below remain the authority for both the block placeholders (approximated in low-resolution pixel textures) and the final art. The camera is first person by default, switchable to third person, not the fixed isometric camera this document assumed on 2026-09-20 or earlier — see "Camera" below. The 2026-09-20 mixed 2D/3D character decision is superseded; see the "Superseded — 2D layered character pipeline" section near the end of this document, which is kept for history rather than deleted.

## Visual promise

A school bag against an impossible sky. Everyday India is warm, crowded and specific; the Gate is an interruption of familiar material and space. The normal world must remain worth spending time in. The final art (from M9) uses stylised 3D, clean silhouettes and painted surface variation; during development the same spaces are built from blocks in the same palette, per `Docs/ASSET_PIPELINE.md`.

Final characters are fully 3D stylised meshes on the standard Unreal humanoid skeleton — the same skeleton block characters use during development, so animations built earlier carry over without rework (`GAME_PLAN.md` §3, rule 4). Illustrated close-up portraits continue as before, independent of which presentation layer (block or final mesh) is active. Characters are original contemporary Indian people, with adolescent proportions at 16. Do not give school students adult fashion-model proportions or default every outfit to ceremonial dress.

### Camera (added 2026-09-23)

First person is the default for walking, interacting, working, studying and fighting; third person is available at any time by toggle and must be fully supported, not a debug view. The appearance editor uses an orbit camera; authored scenes may frame shots; mirrors and shop windows show the player in first person. This holds for both the block presentation during development and the final art at M9 — it is a camera and control decision, not an art-style one. See `GAME_PLAN.md` §3.

Kolkata's first district is a fictional Kolkata neighborhood. Use a consistent built environment: brick and plaster retaining walls, plastered low-rise homes, shaded verandas, practical school buildings, monsoon drainage, repair shops, transit stops, courtyard vegetation and mixed residential/commercial streets. A broad Indian audience does not require every region's iconic building in the same block. Regional consultants and local visual reference collection are production tasks before final meshes and signage.

## Palette and material grammar

| Area | Main colors | Surface and light | Readability rule |
|---|---|---|---|
| Home | Sand, cream, muted terracotta | Worn plaster, cotton, warm daylight | Personal objects establish family life |
| School | Chalk blue, navy, warm white | Painted walls, concrete, notebooks | Clear doors and interactable desks |
| Market | Warm ochre with limited bright accents | Metal shutters, tarpaulins, wet paving | Shopfront silhouette before tiny signage |
| Night street | Indigo and amber | Small pools of practical light | Traversal remains readable |
| Gate | Teal, violet, pale luminous edges | Broken geometry, displaced reflections | FX must not obscure attack telegraphs |
| Prithvi/Jala/Agni/Vayu/Akasha | Earth/blue/ember/pale green/violet | Each has a distinct motion language | Add shape and sound; never color alone |

## Reference images in Art/

kolkata-nabapara.png: environment key art. Establishes atmosphere and district density; it is not a surveyed level map.

student-lineup.png: four school-age protagonist costume/silhouette directions. This is concept exploration, not the complete avatar customization catalog.

Generated images are noninteractive reference art. Their lettering, geometry and detail require human cleanup and region-specific review before production. Source prompts and generation notes are recorded in ART_PROVENANCE.md. The working title is inherited from the supplied concept and remains provisional pending a separate naming/clearance process before public release.

## Character construction

**Updated 2026-09-23.** Final characters are fully 3D stylised meshes built on a tested base skeleton family (the standard Unreal humanoid skeleton), with varied proportions and skinned morph targets, arriving at milestone M9. During development, the same character-creation contract (CHARACTER_CREATION.md) is rendered as a block character — cuboid parts on that same skeleton — so every animation authored during development carries over unchanged. The 2026-09-20 "layered 2D character editor" decision that previously headed this section is superseded; it is preserved below under "Superseded — 2D layered character pipeline" rather than deleted.

Continuous colour sliders (skin tone, hair colour, eye colour) tint the block or final material at runtime; hair style, face shape, build, and uniform/outfit variant are discrete picks between authored options (block parts during development, mesh/morph variants at M9), not sliders. Proportion and facial-feature sliders wait for the final skinned rig, where morph targets make them cheap — this split, and the reasoning behind it, is recorded in CHARACTER_CREATION.md.

Skin tone, face, hair, gender presentation and disability/accessibility representation must not alter point costs or supernatural potential. School dress variants, casual home wear and rain accessories form the first clothing set. Clothing can express personality without overriding school context. Paid presentation rank represents learned self-presentation, not a beauty score. Visually distinguish school bag, practice bag and earned Gate equipment.

Named NPC portrait priorities: guardian, closest classmate, class representative, teacher, tea-stall owner, clinic worker, training supervisor and Gate researcher. Four useful expressions per priority character: neutral, warm, concerned, determined. Check each at small phone-dialogue size. Use narrative characterization and posture rather than ethnicity or poverty as villain shorthand.

## Final-art asset backlog

**Updated 2026-09-23.** This backlog describes the M9 real-art commissioning target. Development-time block placeholders — generated from the same asset IDs, so every row below already has a working block version to build and test against — are specified in `Docs/ASSET_PIPELINE.md`, not here.

| Package | First batch | Completion definition |
|---|---|---|
| Avatar | One fully 3D stylised character (base skeleton, uniform, hair, face) with four face/build options, four hair-style options, two school dress variants, casual outfit; tintable skin/hair materials for the colour sliders | Reads correctly across every option combination; idles, turns and interacts without seams at camera distance, in both first- and third-person |
| Home | One small interior with study and family interaction props | Visible household variation from origin context |
| School | Gate, courtyard, classroom, noticeboard, corridor kit | Blockout route and event staging pass before polish |
| Streets | Facade kit, shutters, awnings, drains, curb/road modules | Reusable without obvious repeated landmark silhouettes |
| Props | Desk, chair, notebook, bag, cycle, umbrella, tea counter, bench | Collision, LODs and interaction sockets reviewed |
| Gate | Portal, modular3-room kit, one landmark, exit cue | Clear navigation and performance-friendly materials |
| Combat | One weapon set, two enemy silhouettes, one boss silhouette | Windup/impact/recovery readable without VFX |
| Interface | Creation components, budget meter, trait cards, school planner, phone shell | Text scales and keyboard/controller/touch focus works |
| Audio | Street bed, school bell, rain, footsteps, UI feedback, Gate motif | Volume hierarchy supports dialogue and accessibility |

Counts are a commissioning backlog, not supplied finished assets. Keep editable sources alongside exports once artists create them. Define skeleton, meter scale, pivot conventions and texture limits with the technical artist before commissioning a large batch.

## Startup menu and creation screen brief

**Design decision, 2026-09-20 — title screen and flow order; display updated 2026-09-23.** Title screen: New Game, Load Game, Settings, Exit. Background: quiet morning view toward the school, with a subtle distant Gate. No combat spectacle before the ordinary-life premise is readable. New Game opens the appearance editor first, then point allocation (including household), then starts play in the room the confirmed household template and Household Resources rank determine (see `Docs/ROADMAP.md`). This reorders the previous "New Life" flow below, which put identity/family ahead of appearance. The flow order is unchanged by the 2026-09-23 voxel-block decision; only the appearance step's rendering changed, from the description below to a block character in an orbit camera (`GAME_PLAN.md` §4, Phase 1).

New Game steps: Identity → **Appearance (block character editor during development; full 3D character editor at M9)** → Family and Home (household template, or institutional care at Household Resources Rank 0) → Talents and Experience → Review. A persistent header shows Age16 • Class XI • Kolkata and Points remaining /1000. Desktop uses avatar preview left, choice cards center, consequences and remaining points right; the preview reflects the appearance editor's live parameter values from the moment that step is reached. Mobile uses one panel at a time, sticky budget summary and a collapsible preview. Every rank card shows cumulative cost, next increment and plain-language effect. Undo, Reset and Presets remain reachable. Over-budget upgrades explain the shortfall without erasing the draft.

Review shows family starting context, school routine, strengths and areas to grow. Confirm does not imply choosing a permanent combat class. Intro camera moves from the bedroom study desk to the school commute. Character creation can be edited freely before beginning life.

## Art review gates

Review1: district coherence and narrative readability at thumbnail size. Review2: age, costume, everyday detail and regional feedback. Review3: greybox comparison verifies the image can inform real traversable space. Review4: in-engine mobile render and shader cost. Review5: UI legibility, subtitle backgrounds, focus state, safe areas and color-independent cues. Final images support these decisions; they do not replace the checks.

## Superseded — 2D layered character pipeline (design decision of 2026-09-20, superseded 2026-09-23)

This section is kept as the historical record of the 2D/2.5D presentation decision rather than deleted. It no longer describes any current plan: neither the development-time block characters (`GAME_PLAN.md` §3, `Docs/ASSET_PIPELINE.md`) nor the final stylised 3D characters at M9 (see "Character construction" above) use layered 2D art.

**Design decision, 2026-09-20 — mixed 2D/3D presentation (superseded).** Characters were assumed to be displayed through a separate, replaceable layer that composed anime-styled 2D/2.5D sprite art into the 3D world, swappable later for a 3D skeletal mesh rig. The environment, camera (then a fixed isometric camera, itself since superseded — see "Camera" above), and world stayed 3D and authoritative throughout.

**Design decision, 2026-09-20 — layered 2D character editor, not a base skeleton first (superseded).** The first playable presentation was to be a slider-and-picker character editor building anime-styled 2D/2.5D characters from layered art. A tested base skeleton family, varied proportions, and skinned 3D meshes were deferred to a later 3D presentation layer. Continuous colour sliders (skin tone, hair colour, eye colour) would tint art at runtime; hair style, face shape, build, and uniform/outfit variant were discrete picks between authored layer options — reasoned as necessary because a 2D layered sprite cannot cheaply deform its silhouette the way a 3D mesh can, so proportion and facial-feature sliders would wait for the eventual 3D rig's morph targets.

**Layered character art specification (superseded, not to be commissioned from).** The editor was to composite a fixed stack of transparent-PNG layers (base body, uniform/outfit, hair back, head/face, hair front, accessories) sharing a common canvas size and aligned anchor points (head center, shoulder line, eye line, hip line), so any combination of layer variants composed without seams or drift. Layers a colour slider must tint (skin, hair) would be supplied as flat/greyscale tintable art; layers not slider-tintable (uniform colour, printed patterns, eye iris art) would be supplied fully pre-coloured. The itemised version of this brief (exact canvas size, pivot coordinates, naming examples, per-stage variant counts) was delivered to the owner on 2026-09-20 and is not to be attached to any active commissioning brief now — commissioning follows "Final-art asset backlog" above and `Docs/ASSET_PIPELINE.md` instead.
