# Art direction and production brief

## Visual promise

A school bag against an impossible sky. Everyday India is warm, crowded and specific; the Gate is an interruption of familiar material and space. The normal world must remain worth spending time in. Environments use stylized3D with a fixed isometric camera, clean silhouettes and painted surface variation, exactly as before.

**Design decision, 2026-09-20 — mixed 2D/3D presentation.** Characters are no longer assumed to be 3D meshes by default. The environment, camera, and world stay 3D and authoritative (see TECHNICAL_ARCHITECTURE.md's presentation-layer decision); characters are displayed through a separate, replaceable layer that today composes anime-styled 2D/2.5D sprite art into that 3D world, and later can be swapped for a 3D skeletal mesh rig without changing the world, the camera, or any rule. This is a deliberate step toward eventual full 3D, not an abandonment of it: building the swap boundary now means the later move to full 3D characters is a presentation-layer replacement, not a rewrite. Illustrated close-up portraits continue as before, independent of which display mode is active. Characters are original contemporary Indian people, with adolescent proportions at16. Do not give school students adult fashion-model proportions or default every outfit to ceremonial dress.

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

**Design decision, 2026-09-20 — layered 2D character editor, not a base skeleton first.** The first playable presentation is a slider-and-picker character editor building anime-styled 2D/2.5D characters from layered art (see CHARACTER_CREATION.md for the full slider/picker scope). A tested base skeleton family, varied proportions, and skinned 3D meshes remain the plan for the later 3D presentation layer, once that layer is built; they are not the first deliverable and this document no longer assumes 3D character meshes exist from the start.

In the first version, continuous colour sliders (skin tone, hair colour, eye colour) tint art at runtime; hair style, face shape, build, and uniform/outfit variant are discrete picks between authored layer options, not sliders — a 2D layered sprite cannot cheaply deform its silhouette the way a 3D mesh can, so proportion and facial-feature sliders wait for the 3D rig, where morph targets make them cheap. This split, and the reasoning behind it, is recorded in CHARACTER_CREATION.md and must not be treated as a simplification the art brief below can quietly ignore.

Skin tone, face, hair, gender presentation and disability/accessibility representation must not alter point costs or supernatural potential. School dress variants, casual home wear and rain accessories form the first clothing set. Clothing can express personality without overriding school context. Paid presentation rank represents learned self-presentation, not a beauty score. Visually distinguish school bag, practice bag and earned Gate equipment.

### Layered character art specification (for commissioned art)

This is the concrete brief an artist works from (the full itemised version, with exact canvas size, pivot coordinates, naming examples and per-stage variant counts, was delivered to the owner alongside these document updates and should be attached to the actual commissioning brief). In summary: the editor composites a fixed stack of transparent-PNG layers (base body, uniform/outfit, hair back, head/face, hair front, accessories) that must share a common canvas size and a common set of aligned anchor points (head center, shoulder line, eye line, hip line) so any combination of layer variants — one per discrete picker option — composes without seams or drift. Layers that a colour slider must tint (skin, hair) are supplied as flat/greyscale tintable art; layers that are not slider-tintable (uniform colour, printed patterns, eye iris art) are supplied fully pre-coloured. The uniform must match this game's Kolkata/West Bengal setting — navy and light blue, consistent with the supplied reference sheet's uniform colours — independent of that sheet's non-canonical background (see ART_PROVENANCE.md).

Named NPC portrait priorities: guardian, closest classmate, class representative, teacher, tea-stall owner, clinic worker, training supervisor and Gate researcher. Four useful expressions per priority character: neutral, warm, concerned, determined. Check each at small phone-dialogue size. Use narrative characterization and posture rather than ethnicity or poverty as villain shorthand.

## Slice asset backlog

| Package | First batch | Completion definition |
|---|---|---|
| Avatar | One layered 2D character (base body, uniform, hair front/back, face) with four face/build picker options, four hair-style picker options, two school dress variants, casual outfit; tintable skin/hair layers for the colour sliders | Composes without seams across every picker combination; idles, turns and interacts without visible layer drift at camera distance |
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

**Design decision, 2026-09-20 — title screen and flow order.** Title screen: New Game, Load Game, Settings, Exit. Background: quiet morning view toward the school, with a subtle distant Gate. No combat spectacle before the ordinary-life premise is readable. New Game opens the appearance editor first, then point allocation (including household), then starts play in the room the confirmed household template and Household Resources rank determine (see NEXT_SMALL_STEPS.md). This reorders the previous "New Life" flow below, which put identity/family ahead of appearance.

New Game steps: Identity → **Appearance (full 2D character editor)** → Family and Home (household template, or institutional care at Household Resources Rank 0) → Talents and Experience → Review. A persistent header shows Age16 • Class XI • Kolkata and Points remaining /1000. Desktop uses avatar preview left, choice cards center, consequences and remaining points right; the preview reflects the appearance editor's live parameter values from the moment that step is reached. Mobile uses one panel at a time, sticky budget summary and a collapsible preview. Every rank card shows cumulative cost, next increment and plain-language effect. Undo, Reset and Presets remain reachable. Over-budget upgrades explain the shortfall without erasing the draft.

Review shows family starting context, school routine, strengths and areas to grow. Confirm does not imply choosing a permanent combat class. Intro camera moves from the bedroom study desk to the school commute. Character creation can be edited freely before beginning life.

## Art review gates

Review1: district coherence and narrative readability at thumbnail size. Review2: age, costume, everyday detail and regional feedback. Review3: greybox comparison verifies the image can inform real traversable space. Review4: in-engine mobile render and shader cost. Review5: UI legibility, subtitle backgrounds, focus state, safe areas and color-independent cues. Final images support these decisions; they do not replace the checks.
