# Asset pipeline

**Document status:** plan, 2026-09-23. Authoritative, under `GAME_PLAN.md`, for the block style, asset IDs, presentation contracts, the asset registry, placeholder generation, the resolver, both import routes, provenance and the M9 art validation. **Nothing here is implemented.** Where `ART_DIRECTION.md`, `TECHNICAL_ARCHITECTURE.md` or `ART_PROVENANCE.md` describe 2D layered sprites, an isometric camera, baked lighting or `CH_`/`ENV_` prefixes, this document supersedes them for development-time assets.

**The contract in one paragraph.** Gameplay names every visual and sound by asset ID. The ID's registry entry holds real-metric dimensions, a presentation contract, gameplay data (collision, anchors, seats, sockets) and a block model, so every ID is visible and playable the moment it is defined. Real art fills empty slots at M9, through a drop folder or the editor, under one validator. Stable IDs preserve references; contracts preserve perception; M9 proves both by test rather than assumption.

Units are centimetres and Unreal axes (+X forward, +Y right, +Z up). Values marked *(tune)* are starting points for the M1 look review.

## 1. Block style guide

### 1.1 What "block" means

- Every visible object is a set of cuboids — axis-aligned by default, rotated per cuboid only for roofs, ramps and fallen objects — textured with nearest-filtered pixel tiles. No bevels, smoothing, normal maps or photo textures.
- They are ordinary Unreal static, instanced and skeletal meshes. No voxel terrain, voxel plugin or destruction.
- Everything is real metric size. The grids below keep the pixel look tidy; gameplay never reads them.

### 1.2 Texel classes

| Class | Used for | Density | One texel | Preferred snap | 32 px tile covers |
|---|---|---|---|---|---|
| A | architecture, ground, roads, terrain, water | 20 px/m | 5 cm | 5 cm (structure 25 cm) | 1.6 m |
| P | furniture, fixtures, stalls, vehicles, trees | 40 px/m | 2.5 cm | 2.5 cm | 0.8 m |
| S | small and held items, food, signage | 80 px/m | 1.25 cm | 1.25 cm | 0.4 m |
| C | characters, clothing, hair, faces | 80 px/m | 1.25 cm | 1.25 cm | 0.4 m |

- The densities are 1 : 2 : 4, so texels nest and neighbouring classes read as one world. One object never mixes classes.
- They are metric-friendly: an Indian modular brick with mortar (20 × 10 cm) is 4 × 2 texels in class A, a 2.1 m door is 42, a 3.0 m storey is 60.
- The real dimension wins over the snap. A cuboid that is not a texel multiple keeps its real size and the world-aligned tile crops its last texel.
- Reference dimensions: walls 25 cm (23 cm brick plus plaster); doors 90 × 210; windows 120 × 135, sill at 75, a 45 cm chhajja (concrete sunshade) above; storeys 3.0 m in flats, 3.6 m in older houses and the school; school structural bay 4 m (`INDIA_WORLD_BIBLE.md`); stair riser 17.5, tread 25; corridors ≥ 120 wide; ceiling-fan blades ≥ 240 above the floor; ceilings everywhere indoors.

### 1.3 Textures

- **Two mapping modes per face.** *World*: the tile repeats at the class density from cuboid-local coordinates. *Fit*: one image covers the face at the class density (faces, doors, signs, posters).
- **Sizes.** Repeating tiles are 32 × 32 px, packed into one generated `Texture2DArray` (≤ 512 layers). Fit images are power-of-two canvases of 16–256 px whose painted area equals face size × density (±1 texel). Character part atlases are 128 × 128. Nothing in the block world exceeds 256 px.
- **Palette discipline.** Tiles are drawn in any indexed pixel editor (Aseprite recommended) with the master palette loaded. The validator rejects any pixel that is not a palette colour, one of the four tint keys, or fully transparent.
- **Tint keys.** A tile is either fixed-colour or fully tintable, never mixed. Tintable tiles use only the pure greys 64/128/192/255, which the material maps to steps 0–3 of a palette ramp chosen per face (paint colours, uniforms), or to a free colour (skin, hair, eyes). A face may stack one fixed-colour cutout overlay (window frame, stain, sign) on a tintable base. One brick tile therefore serves every paint colour.
- **Unreal settings.** A project texture group (`TEXTUREGROUP_Project01`, "PW Pixel") with point min/mag/mip filtering and simple-average mips; sRGB on; stored uncompressed (BGRA8), because block compression smears pixel art (a 32 × 32 tile with mips is 5 KB). ASTC 4 × 4 for Android after a visual check.
- **Anti-aliasing.** Proposal: the desktop forward renderer with 4× MSAA, which keeps texels crisp and cuboid edges clean. Compare it against deferred shading with TSR using identical M1 captures, because temporal methods soften nearest-filtered textures. Motion blur, film grain, chromatic aberration, lens flare and vignette are off.

### 1.4 The master palette (PW-128)

The palette has 32 ramps × 4 steps and is stored as one 32 × 4 px PNG (`Art/Blocks/palette.png`) with names in `Art/Blocks/palette.json`. 24 ramps are assigned, 2 are reserved for the Gate and 6 are free for later regions (M7). Step 2 is the base. Step 1 is −18% lightness with hue shifted 6° toward blue, step 0 is −36% and 12°, step 3 is +14% and 6° toward yellow, all hand-tuned after. Shadows are cooler and lights warmer, never just darker or lighter.

| Ramp | Base | Kolkata use |
|---|---|---|
| `ink` | #262633 | shoes, iron grills, text plates |
| `paper` | #EFE8D8 | whitewash, paper, cotton, sclera |
| `plaster_cream` | #E2CDA3 | warm plaster, most house walls |
| `plaster_ochre` | #D6A64F | lime-wash yellow, older facades, the market |
| `plaster_mint` | #A8C6A2 | faded pastel-green facades |
| `plaster_rose` | #D4A29B | faded pink facades |
| `chalk_blue` | #A9C3D1 | school and clinic walls |
| `terracotta` | #B45F3C | brick, clay tea cups (bhar), roof tiles, pots |
| `red_oxide` | #8D3A2D | polished red-oxide floors, sills, steps |
| `jhilmil_green` | #2F6A4E | louvred shutters, railings, old doors |
| `monsoon_leaf` | #4E8E3A | banyan, grass and creepers after rain |
| `moss` | #707A3B | moss at plaster edges, algae, khaki |
| `school_navy` | #263B67 | uniform trousers or skirt, school signage |
| `school_sky` | #9FC4E6 | uniform shirt, school accents |
| `concrete` | #A29D93 | slabs, chhajjas, stairs, pavements |
| `asphalt` | #4A4D56 | roads (wet uses steps 0–1) |
| `steel` | #7C8995 | shutters, steel almirahs, pipes, bus panels |
| `teak` | #8A5A35 | furniture, door frames |
| `jute` | #C7A46B | jute sacks, cane, bamboo, rope |
| `taxi_yellow` | #F0C12F | taxis, road markings, sign accents |
| `signal_red` | #C4352D | signs, post boxes, fire equipment |
| `river` | #5E7E84 | canal and river water, puddles |
| `marigold` | #E6891F | flower stalls; festival garlands only in reviewed contexts |
| `dusk_indigo` | #2F305E | night sky tint, night signage |
| `gate_teal` | #2FD1C4 | **reserved:** Gates and awakened effects only |
| `gate_violet` | #8A4FE0 | **reserved:** Gates and Akasha only |

**Rules.** At most three ramps plus `ink` and `paper` per prop. Saturated accents cover ≤ 10% of a typical view. The reserved ramps never appear in the ordinary world, so a teal-violet pixel always means Gate — backed by shape and sound, never colour alone. Saffron/green/white is not used as national shorthand. Skin, hair and eye colours are the one exception to the palette: the player's slider values are used exactly.

### 1.5 Lighting that keeps blocks readable

- There is no Lumen, ray tracing or lightmap baking in the baseline (`Config/DefaultEngine.ini` already disables GI and reflections; `MASTER_PLAN.md` rules them out). The clock moves the sun 15° per game hour, which is one real minute, so lighting is fully dynamic and `TECHNICAL_ARCHITECTURE.md`'s baked-light prototype no longer applies.
- **Sun and sky.** A movable directional light with 3 shadow cascades to 80 m, the first ending at 8 m, and a 0.5° source angle for crisp block shadows. SkyAtmosphere with a movable real-time-capture sky light. Height fog for monsoon haze, so distant silhouettes step back.
- **Face shading.** The block material multiplies each face by a fixed factor from its world normal: top 1.00, ±X 0.90, ±Y 0.80, bottom 0.65 *(tune)*. Every edge reads even in flat light.
- **Baked corner AO.** The baker writes per-vertex occlusion from neighbouring cuboids into vertex colour (0.70 at a closed corner), standing in for the missing GI in corners and under furniture.
- **Ratios.** Exterior daytime key-to-fill is 3 : 1. Interiors keep fill ≥ 25% of key and are never black: sky fill, practicals (tube lights 6500 K, bulbs 2700 K, amber 3000 K street lights against `dusk_indigo`), and a post-process volume per interior with exposure clamped to ±0.5 EV of its target.
- **Faces in conversation.** A camera-attached face fill (lighting channel 1, 15% of key) lights only characters, so a face is never lost in shade.
- **Weather.** `Wetness` darkens albedo by one ramp step and raises specular; puddles are flat cuboids reflecting the sky capture. Lumen may later become an optional Windows "high" setting; nothing may depend on it.

### 1.6 First person, faces, UI and text

- **Faces.** A 16 × 16 texel window (20 × 20 cm) on the front of the head. At the 1.2–1.6 m conversation distance, a 90° FOV and 1080p, one face texel is about 8 screen pixels. Expressions and identity are in §2.3.
- **Camera.** The first-person camera follows the capsule at eye height, never the animated head bone, so no animation adds head motion. Conversation framing is optional and released by any look input. Subtitles name the speaker and point toward off-screen speech.
- **Targeting.** A 2.0 m sphere trace from the camera with an 8° acquisition cone and 0.3 s stickiness. Overlaps resolve by the anchor's data `priority`, then angle to screen centre, with a cycle key. Small items get a data-defined target radius (≥ 10 cm). The target shows a 1-texel outline (custom depth stencil, `paper` at 60%) and a prompt with icon and verb text. Nothing requires precise aim.
- **UI.** Flat, not 3D blocks: pixel-art 9-slice frames with 2 px borders at integer-friendly scales (a DPI rule snapping to 1×, 1.5×, 2× or 3× of a 1280 × 720 base; 2 px × 1.5 stays whole). The phone opens as a flat, screen-aligned, full-resolution view, never a render target on the block phone.
- **Text.** Body text, subtitles and all Bengali use Noto Sans and Noto Sans Bengali (SIL OFL). A pixel display face is allowed only for Latin digits and short headings. Minimum sizes at 1080p: body 20 px, prompts 22 px, subtitles 28 px by default (20–48). Text sits on `ink` plates at 70% opacity with contrast ≥ 4.5 : 1.
- **Signs.** In-world signs carry flavour, never sole information. Sign text is a separate fit layer per language (`.bn`, `.en`), authored by people, never machine-translated or AI-lettered.

### 1.7 Charm and tone

- **Charm comes from specific silhouettes, not detail.** Chhajjas over windows, louvred shutters, balcony grills as cutout tiles, utility lines as 2.5 cm cuboids, laundry, potted plants, bicycles, the banyan. Every building has at least one overhang or lip, and every room at least one personal object. No gradients or noise; an ordered 2 × 2 dither only for stains and wear; no lighting painted into tiles.
- **Tone.** The presentation bible (`GAME_PLAN.md` §3) governs injury, poverty, institutional care, grief, intimacy and violence. Blocks can make a scene comic that detail would make cruel, so the block rules are: no comic physics on bodies (no ragdoll flop, pop, squash or cartoon sound); injury through bandage, sling, posture, limp and sound, never red-cube gore; poorer homes and the children's home get the same palette, saturation and lighting care as any home — tidy, used and cared for, never a grime filter; intimacy through framing and posture only; violence readable by wind-up and posture.
- **Tone tags.** Assets and scenes carry `tone.*` tags (`injury`, `poverty`, `care`, `grief`, `intimacy`, `violence`). Every tagged scene has a scripted capture that M9 re-reviews with final art.

## 2. Block characters

### 2.1 Skeleton and proportions (frozen)

- **Skeleton.** `skeleton.humanoid@1` is the UE 5.7 mannequin skeleton (`SK_Mannequin`: root, pelvis, spine_01–05, neck_01–02, head, clavicle/upperarm/lowerarm/hand with metacarpals and fingers, thigh/calf/foot/ball, twist and IK bones) plus seven sockets: `hand_r_grip`, `hand_l_grip`, `hand_r_pinch`, `hand_l_pinch`, `back_bag`, `head_ear_r`, `hip_r`. Its bone names, hierarchy and reference pose are hashed into `Data/Presentation/skeleton_humanoid_v1.json` and frozen at the M1 gate. Any change is a new version plus a full contact-test rerun.
- **Proportions policy.** A character is the reference pose scaled uniformly, height band 1.45–1.95 m (scale = target height ÷ reference height, measured at M1). Build changes cross-sections, never bone lengths, so every animation stays valid. Final art must keep joint positions within ±3% of the scaled reference; beyond that it needs a retarget profile and passes the contact tests again. Children get a separate `proportions.child@1` profile, defined before the first child appears. Until the AppearanceRecord has a height field, the player is 1.65 m.

### 2.2 Construction and appearance mapping

Each part is a cuboid bound 100% to one bone, which makes a rigid-skinned mesh. Lengths come from the reference pose (bone to child bone), plus 1.25 cm of overlap at each joint so bends never open gaps. Cross-sections are for `build_average`:

| Part | Bone | W × D | Length | Part | Bone | W × D | Length |
|---|---|---|---|---|---|---|---|
| hips | pelvis | 32.5 × 20 | 15 | palm ×2 | hand_l/r | 8.75 × 3.75 | 8.75 |
| belly | spine_02 | 30 × 18.75 | to spine_04 | finger mitten ×2 | middle_01_l/r | 8.75 × 3.75 | 7.5 |
| chest | spine_04 | 35 × 21.25 | to neck_01 | thumb ×2 | thumb_01_l/r | 2.5 × 2.5 | 5 |
| neck | neck_01 | 10 × 10 | to head | thigh ×2 | thigh_l/r | 13.75 × 13.75 | to calf |
| head, jaw, hair | head | by face shape | — | shin ×2 | calf_l/r | 11.25 × 11.25 | to foot |
| upper arm ×2 | upperarm_l/r | 10 × 10 | to lowerarm | foot ×2 | foot_l/r | 10 × 7.5 | 17.5 |
| forearm ×2 | lowerarm_l/r | 8.75 × 8.75 | to hand | toe ×2 | ball_l/r | 10 × 5 | 7.5 |

The other bones carry no geometry. A character is about 35 cuboids and 420 triangles.

**AppearanceRecord to parts**, from `Data/Presentation/appearance_mapping_v1.json`:

| Choice | Block result |
|---|---|
| `face_round` | head 21.25 W × 21.25 H × 22.5 D; corner texels of the face window rounded |
| `face_oval` | head 20 × 23.75 × 22.5 (the default) |
| `face_square` | head 21.25 × 21.25 × 22.5, plus a jaw block 21.25 × 3.75 × 18.75 beneath, set 1 texel behind the face |
| `face_heart` | head 21.25 × 20 × 22.5, plus a chin block 12.5 × 3.75 × 15 beneath |
| `hair_short_crop` | cap (head + 2.5 cm each side, 7.5 tall) and back plate to 60% of head height: 2 cuboids |
| `hair_wavy_bob` | cap, side and back panels to the jaw line, alternating-texel lower edge: 4 cuboids |
| `hair_long_braid` | cap, back plate, 3 braid segments of 5 × 5 × 12.5 (first on `head`, two overlapping on `spine_05`), tie in an accent ramp: 6 cuboids |
| `hair_curly_loose` | cap enlarged 2.5 cm, 6 curl cubes of 5 cm jittered by 1 texel: 8 cuboids |
| `build_slight` / `build_average` / `build_sturdy` | torso, thigh and upper-arm cross-sections ×0.875 W and ×0.9 D / ×1 / ×1.125 both (forearms and shins ×1.06), rounded to 1.25 cm |
| `uniform_school_navy` | `school_sky` shirt with short sleeves ending at 55% of the upper arm, 1-texel `paper` collar, 2 × 2 fictional crest; `school_navy` trousers; `ink` belt and shoes; `paper` socks |
| `uniform_casual` | T-shirt tinted from a casual ramp set (player default `terracotta`), `asphalt` track pants, `teak` sandals over visible skin |

- **Colours.** The three colours become custom primitive data, not per-character material instances. `SkinTone`, `HairColor` (hair and brows) and `EyeColor` (iris) are converted with `FLinearColor::FromSRGBColor`, then shaded through tint steps ×0.62/0.78/0.90/1.00, with the two darker steps warmed (+4% R, −4% B) so skin shadows never turn grey. Clothing uses palette ramp indices; expression frames and pupil offsets are primitive data too.
- **Colour space.** The record's 0–255 values are sRGB. The current Unreal bridge divides by 255 into `FLinearColor`, which treats them as linear, so the creator and the in-world character will disagree until one convention is fixed at M1.
- **Mapping versioning.** The save stores only the AppearanceRecord; mapping files are versioned and immutable once used. Each option carries semantic descriptors (`hair_long_braid`: length mid-back, single braid, centre parting, low volume) and colour roles. The final-art mapping (v2, M9) must keep every option's descriptors and colour roles identical, and its silhouette metrics within tolerance (height ±2%, head width ±10%, same hair-volume class). A validator diffs v1 against v2. The owner reviews a capture of all 96 option combinations × 5 skin tones, block beside final.
- **Portraits** (phone, save slots, milestone photographs) are automatic head-and-shoulders captures of the character, so they re-render themselves at M9.

### 2.3 Faces, expression and identity

Expression channels live in `Data/Presentation/expressions_v1.json` and are referenced by ID from dialogue. Final art maps the same IDs to facial poses.

| Channel | Block states | Driven by |
|---|---|---|
| gaze | 9 pupil offsets (−1/0/+1 texel on each axis) | look-at target |
| lids | open, half, closed, squint; blink every 3–6 s | expression, fatigue |
| brows | neutral, raised, furrowed, sad, one raised | expression |
| mouth | closed, smile, open smile, frown, pressed, open; visemes closed/half/open at 8–10 Hz | expression, speech |
| head angle | yaw ±70°, pitch ±35°, roll ±15° across neck_01, neck_02 and head | look-at, expression |
| posture | upright, relaxed, slumped, guarded, leaning in, arms crossed | expression, condition |

- **Named expressions** are channel tuples: neutral, warm, concerned and determined (the four in `ART_DIRECTION.md`), plus surprised, sad, angry, tired, laughing and embarrassed. Each uses at least two channels, one of them head angle or posture, so it reads at 5 m and when the face is small.
- **Multi-channel meaning.** Consequential information never depends only on a small facial change, a colour, precise aiming or forced camera movement. It is always also carried by dialogue text, sound, posture or UI.
- **Identity kit** for tier 0 and tier 1 NPCs: a silhouette (hair, height band, build and one silhouette accessory — glasses, dupatta, cap, apron, bag, walking stick); a clothing signature of two ramps; and habitual movement (gait speed, arm swing, posture preset, one idle habit).
- **Identity checks.** The validator requires any two named cast members to differ in at least two of hair, height band, build, accessory and primary ramp. Playtest: after one in-game day, with nameplates off, testers identify at least 7 of the 8 slice cast at 12 m and in back-lit silhouette.

### 2.4 Hands, contact and the first-person viewmodel

- **Grips** (`grip.*`). Each is a mitten/thumb angle pair on blocks and a full finger pose on final art: `power` 90°/45°, `pinch` 30°/60°, `pen`, `cup` (a clay bhar held around its body), `phone`, `hook` (bag handle), `flat_support` (plate on the palm), `two_hand_carry`, `tool_twist` (screwdriver, spanner), `eat_hand` (eating with the right hand, shown as ordinary), `help_hand` (hand to hand, hand to elbow).
- **Reach and seating.** Standing anchors sit 35–185 cm above the floor and within 60 cm of the shoulder at the stance point; seated anchors 15–35 cm above the seat and 25–55 cm forward; floor anchors use a crouch. Seat heights: chair 45, bench 42, stool 30, bed edge 45, floor 0. Knee clearance under desks is at least 60 high × 45 deep.
- **Held items.** Each carries a `grip` socket and a grip type and attaches to `hand_r_grip` (or the left), with one transform shared by both cameras.
- **First/third-person correspondence.** Every interaction animation is a pair under one ID (`anim.interact.write_desk` → `fp`, `tp`). Both play for the duration given in data, with the same notifies (`contact_begin`, `contact_end`). Animation is presentation only: the command's duration and outcome come from data, never from clip length.
- **Contact tolerances.** Hand to anchor ≤ 3 cm in third person; visible hand to item ≤ 1.5 cm in first person; pelvis-to-seat gap ≤ 2 cm; penetration ≤ 2 cm; foot slide ≤ 3 cm per step; held item into body ≤ 1 cm.
- **Contact scenarios.** Scripted, automated, and run at minimum, average and maximum proportions × three builds: writing at a school desk; eating a meal seated on the floor, by hand; holding tea at the chai stall; carrying a school bag, a water bucket, and books two-handed; repairing a tap with a spanner; fixing a fan from a stool; helping a fallen person up; supporting an elderly person's elbow; raising and using the phone; opening a door; sitting on a bus seat.
- **Viewmodel.** `char.fp.arms` is generated from the same parts: the upper arm from the sleeve line down, forearm, palm, mitten and thumb, with sleeve cuff and skin from the current outfit. It uses Unreal's first-person rendering (camera first-person FOV 70°, independent of the 70–110° world FOV slider; primitive type First Person), to be confirmed in 5.7 at M1. The full body is hidden from its owner in first person but still casts a shadow and appears in mirrors and scene captures.

### 2.5 Placeholder animation

Procedural until real animation exists, in one C++ AnimInstance on `skeleton.humanoid@1`:

- **Locomotion.** Thigh swing ±25° at walking speed (1.4 m/s), rising to ±35° running (3.5 m/s). Calf bend up to 40° on the back-swing, arm counter-swing ±20°, pelvis bob 2 cm at twice step frequency. Phase advances with distance travelled (0.7 m stride), so feet do not slide.
- **Idle.** Breathing on spine_04 at ±1° and 0.25 Hz; a weight shift every 6–10 s. Look-at uses the head limits above plus pupil offsets.
- **Poses.** Tiny hand-keyed sequences: sit on a chair, sit cross-legged on the floor, sit on a bed edge, lie, crouch, hold the phone, write. Reach uses two-bone IK to the anchor; grips come from the grip table.
- **Talk.** Visemes cycle while a line plays, with small head nods.
- **Alternative.** Epic's free mannequin animations already play on this skeleton and could replace much of this early. That is an owner decision (§12).

## 3. Asset IDs, presentation contracts and the registry

### 3.1 Naming and what gets an ID

IDs take the form `category.subcategory.name` with an optional `.variant`: lowercase ASCII, digits and underscore, 3–4 segments of ≤ 32 characters. Regex: `^[a-z][a-z0-9_]*(\.[a-z0-9][a-z0-9_]*){2,3}$`.

| Category | Gets IDs for | Examples |
|---|---|---|
| `arch` | walls, floors, doors, windows, stairs, roofs, railings, Gate kit | `arch.door.wooden_panel`, `arch.gatekit.floor_shear` |
| `ground` | roads, pavements, drains, terrain, water | `ground.road.asphalt_lane` |
| `prop` | furniture, fixtures, stalls, containers | `prop.furniture.bed_plank_single` |
| `item` | holdable and inventory objects | `item.phone.student_midrange`, `item.cup.clay_bhar` |
| `veh` | vehicles | `veh.road.taxi_yellow`, `veh.road.bus_city` |
| `flora` | trees and plants | `flora.tree.banyan_maidan` |
| `char` | appearance options, named characters, outfits, viewmodel | `char.appearance.face_round`, `char.npc.mira_sen`, `char.fp.arms` |
| `anim` | animations and poses | `anim.loco.walk`, `anim.interact.write_desk` |
| `mat` | surface materials (also drive footsteps and wetness) | `mat.surface.plaster_painted` |
| `tex` | real-art-only textures: signs, posters, decals, portraits | `tex.sign.shanti_chai` |
| `ui` | icons, frames, phone app art | `ui.icon.gate_rank_f` |
| `vfx` | effects | `vfx.gate.rift_edge`, `vfx.weather.rain_splash` |
| `sfx` `amb` `mus` `vo` | effects, ambiences, music, voice | `sfx.ui.confirm`, `amb.street.nabapara_morning` |
| `test` | fixtures and cost proxies, never shipped | `test.proxy.character_hero` |

- **Appearance options map mechanically.** Each ID is `char.appearance.` plus the option ID in `Appearance.cpp`, so no lookup table can drift. The validator fails if any option lacks an entry.
- **What gets no ID.** Pixel tiles (`tile.*`) and palette ramps are block-building material, not assets, and stay out of coverage. Fonts, master materials and engine content get no IDs either.

**Stability rules.** (1) A committed ID is permanent: never renamed, never reused; display names and paths may change. (2) Retirement sets `status: retired` and `replaced_by`; the resolver follows it and logs a warning, so old saves still load. (3) An ID says what a thing is, never how or by whom it is drawn: no `block`, `kenney`, `ai`, `v2` or `final`. (4) Colour and wear are parameters; a variant gets its own ID only when its gameplay differs or both versions appear together. (5) Saves and content store IDs, never paths. (6) Unreal package paths derive from the ID — `/Game/Art/prop/furniture/bed_plank_single/SM_bed_plank_single`, generated blocks under `/Game/Generated/Blocks/`. Unreal has its own "Asset Registry", so code uses `PW` names (`FPWAssetId`, `UPWAssetResolverSubsystem`) to keep the two apart.

### 3.2 Presentation contracts

Each asset family has a versioned contract in `Data/Contracts/<family>.json` (`prop.bed@1`, `prop.seat@1`, `arch.door@1`, `item.handheld@1`, `char.humanoid@1`, …). Each registry entry names its contract; per-asset values override the family defaults.

| Field | `prop.bed@1` |
|---|---|
| dimensions | 80–100 × 180–200 × 35–60 cm; mattress top 40–50 |
| visual envelope | `inner` box the art must fill and `outer` box it must stay inside, both separate from collision |
| collision | boxes. Visibility and Camera channels match visible surfaces within 5 cm; the Pawn channel may fill voids ≤ 40 cm high under furniture |
| interaction anchors | at least one standing anchor on the approach side (+X), on 60 × 60 cm of clear floor |
| camera clearance | first-person camera (12 cm probe) ≥ 15 cm from any visual surface in every anchor and seat pose; the third-person boom (2.5 m, 12 cm probe) pulls in without clipping in a 2.4 × 2.4 m room |
| seated clearance, reach | edge seat with pelvis at 45 cm and ≥ 110 cm headroom above the seat; lie pose 180 cm long |
| skeleton and pose | `pose.sit_edge_45`, `pose.lie_back` on `skeleton.humanoid@1` |
| semantic cues | long low platform with a pillow end; mattress and frame ≥ 2 ramp steps apart; reads as a bed at 15 m and shows its head end at 5 m; creak `sfx.foley.wood_creak` on sitting |
| performance | block: ≤ 300 triangles, 1 draw call. Final: ≤ 6,000 triangles at LOD0 (LOD1 50%, LOD2 20%), ≤ 2 material slots, ≤ 2 × 1024² textures |

- **Contracts are tested in blocks throughout development.** The generator can build `min` (shrunk to the inner envelope), `max` (grown to the outer) and `alt` (a different arrangement, e.g. a bed with a headboard) variants of any block model. Every contact scenario runs against all three and against the proportion extremes. If gameplay works with only one particular block, the contract is wrong.
- **Rendered-versus-collision comparison.** Automated, at every resolver tier: 2,048 rays per asset from a hemisphere of directions and at player heights of 0–180 cm. It fails when a ray meets Visibility or Camera collision ≥ 5 cm before any visible surface (an invisible barrier), or meets a visible surface with no Pawn collision within 10 cm behind it on a family marked solid, ignoring cuboids marked `soft` (furniture the player could walk through). Every interaction anchor must be visible within 20 cm and reachable along a clear Visibility-channel line from its stance point.

### 3.3 Registry format

There is one JSON file per ID at `Data/Assets/<category>/<subcategory>/<name>.json`. The portable core parses and validates it (so CTest covers it) and Unreal reads it. The example is a taktaposh, the wooden plank bed of Bengali homes:

```json
{
  "schema": "pw.asset/1",
  "id": "prop.furniture.bed_plank_single",
  "display_name": "Taktaposh (single plank bed)",
  "status": "active", "contract": "prop.bed@1", "milestone": "M1",
  "tags": ["home", "sleep", "culture.bengal_specific"],
  "dimensions_cm": [90, 190, 55], "pivot": "bottom_center", "front": "+x",
  "envelope_cm": {
    "inner": { "min": [-40, -88, 36], "max": [40, 88, 44] },
    "outer": { "min": [-48, -98, 0], "max": [48, 98, 75] }
  },
  "block": {
    "class": "P",
    "cuboids": [
      { "name": "deck",   "min": [-45, -95, 35],     "size": [90, 190, 5],   "tile": "tile.wood_plank", "tint": "teak" },
      { "name": "leg_fl", "min": [37.5, -95, 0],     "size": [7.5, 7.5, 35], "tile": "tile.wood_grain", "tint": "teak" },
      { "name": "leg_fr", "min": [37.5, 87.5, 0],    "size": [7.5, 7.5, 35], "tile": "tile.wood_grain", "tint": "teak" },
      { "name": "leg_bl", "min": [-45, -95, 0],      "size": [7.5, 7.5, 35], "tile": "tile.wood_grain", "tint": "teak" },
      { "name": "leg_br", "min": [-45, 87.5, 0],     "size": [7.5, 7.5, 35], "tile": "tile.wood_grain", "tint": "teak" },
      { "name": "tosak",  "min": [-42.5, -92.5, 40], "size": [85, 185, 5],   "tile": "tile.cotton_check", "tint": "school_sky",
        "faces": { "-z": "hidden" } },
      { "name": "pillow", "min": [-25, -90, 45],     "size": [50, 30, 10],   "color": "paper.3", "soft": true }
    ]
  },
  "gameplay": {
    "surface": "mat.surface.wood_teak",
    "collision": [
      { "shape": "box", "center": [0, 0, 40],   "half_extent": [45, 95, 5],    "channels": ["visibility", "camera", "pawn"] },
      { "shape": "box", "center": [0, 0, 17.5], "half_extent": [45, 95, 17.5], "channels": ["pawn"] }
    ],
    "nav": { "area": "obstacle" },
    "interactions": [
      { "id": "sleep", "verb": "verb.sleep", "stance": [70, 0, 0], "facing_deg": 180, "seat": "lie", "priority": 1 },
      { "id": "sit", "verb": "verb.sit", "stance": [70, 40, 0], "facing_deg": 180, "seat": "edge_right", "priority": 2 },
      { "id": "store", "verb": "verb.store_under", "stance": [75, 55, 0], "facing_deg": 180,
        "pose": "pose.crouch", "grip": "grip.two_hand_carry", "anchor": [30, 55, 10], "priority": 3 }
    ],
    "seats": [
      { "id": "edge_right", "pose": "pose.sit_edge_45", "pelvis": [30, 40, 45], "facing_deg": 0, "exit": [70, 40, 0] },
      { "id": "lie", "pose": "pose.lie_back", "pelvis": [0, 5, 47], "head": [0, -75, 52], "exit": [70, 0, 0] }
    ],
    "sockets": [
      { "id": "pillow_top", "location": [0, -75, 55] },
      { "id": "under_storage", "location": [0, 40, 0], "half_extent": [35, 45, 15] }
    ]
  },
  "art": { "status": "block", "enabled": true, "mesh": null, "lods": [], "materials": [],
           "skeleton": null, "import": null, "provenance": null }
}
```

**The validator** (portable, run by CTest) checks the schema, the ID regex and that the file path matches the ID; that the contract exists; that cuboids, collision, anchors and seats lie inside the outer envelope; that every tile, ramp, pose, grip and surface is known; and that seats and anchors meet the contract. The Unreal half checks that `art` paths exist when the status is `real`.

## 4. Placeholder generation, budgets and cost proxies

**Recommendation: both generators, from one definition.**

- **Editor-baked blocks (primary).** A C++ commandlet, `-run=PWBakeBlocks`, in a new `PerfectWorldEditor` module. Per ID it merges the cuboids into one `UStaticMesh` (or a rigid-skinned `USkeletalMesh` for character parts), removes hidden internal faces, writes world-scaled UVs, tile layers and tint indices into UV channels, bakes corner AO, and uses one material slot and no mesh collision. Output goes to `/Game/Generated/Blocks/`, stamped with a hash of the definition, tiles and palette. It is deterministic, so bakes are regenerated rather than committed; the build script bakes before cooking and the cook fails on a stale bake. Why primary: one draw call per object, real AO, and the same `UStaticMesh` path real art will take, so the resolver and budgets are exercised honestly.
- **Runtime blocks (fallback and live editing).** `UPWBlockRuntimeBuilder` draws a definition directly as instances of one unit cube, with per-instance custom data (six face tiles and a tint). It has no AO and costs more draw calls, but a saved JSON file hot-reloads in the editor immediately. The owner or an agent can define an ID and walk up to it with no bake step.
- **Every ID is visible at once.** If `block` is omitted, the generator makes one cuboid of the nominal dimensions in the category's default tile, labelled with its ID in development builds. Collision, anchors and seats always come from `gameplay`, so even a bare box is fully playable.
- **Characters.** The runtime path attaches part meshes to an animated skeletal mesh. The baked path uses Leader Pose followers for the player and the editor, and merged meshes (`FSkeletalMeshMerge`) cached per appearance hash for crowds.

**Performance budgets, set before M1** (the owner confirms the hardware). The proposed target is Windows at 1080p60: a 6-core CPU (Ryzen 5 3600 / Core i5-10400 class), a 6 GB GPU (GTX 1660 Super / RX 5600 XT class), 16 GB RAM and an SSD. Android comes later: current mid-range, 30 fps.

| Budget | Final-art total, desktop | Block allowance |
|---|---|---|
| GPU frame | 14 ms (2.7 ms margin to 16.7) | 7 ms |
| Game thread | 12 ms; animation 2.5, NPC presentation 2 | 6 ms |
| Visible draw calls | 2,500 (Android 600) | 1,250 (300) |
| Visible triangles | 4 M | 0.5 M |
| Resident texture memory | 2.0 GB | 150 MB |
| Process memory | 8 GB | 4 GB |
| Characters fully animated within 25 m | 40, plus 60 at reduced update rate | same |
| Shadow-casting lights per view | sun + 4 local | sun + 4 local |
| Interior streaming | ≤ 1.5 s and ≤ 250 MB per location bundle | same |

- **Headroom.** The block world may use half of each budget; the other half is held for final art.
- **Cost proxies.** Instanced cuboids cannot reveal the cost of unique characters, hair, cloth, shadows or transparency. So each contract's performance field also defines a proxy (`test.proxy.*`): `character_hero` (45k triangles, 4 materials, 8 bone influences, 2 × 2048² plus 3 × 1024² textures, masked hair cards, a 1,000-vertex cloth panel), `character_npc` (20k triangles, 3 materials), `prop_large` (8k triangles), `foliage` (masked cards) and `glass` (translucency).
- **Measuring.** `pw.Assets.CostProxy 1` swaps every block for its family's proxy. Every milestone gate records p95 frame time both ways on the benchmark route (school crowd, rain, one Gate) and in the block interior test suite: home, classroom, shop, ward and office at realistic furniture density.

## 5. The resolver

Gameplay code, Blueprints, levels and content hold only `FPWAssetId` values: a struct wrapping the ID, with an editor picker listing the registry. Levels place `APWAssetActor`s carrying an ID, never meshes. `UPWAssetResolverSubsystem::Resolve(id)` is asynchronous and returns the first tier that works:

1. **Real asset.** `art.status` is `real`, `art.enabled` is true, the package loads, and its bounds sit inside the contract envelope.
2. **Baked block.** The generated mesh exists and its hash matches the current definition.
3. **Runtime block.** Built from the definition.
4. **Error cube.** A magenta and `ink` checker cuboid at the nominal size (1 m if unknown), labelled with the ID in development builds. It is logged and counted in coverage, never a crash.

- **Gameplay is tier-independent.** Collision, anchors, seats and sockets are built from `gameplay` at every tier, and visual components have collision disabled. The tier therefore cannot affect physics, navigation or reach.
- **Retired IDs** follow `replaced_by`.
- **Console controls:** `pw.Assets.ForceTier real|baked|runtime`, `pw.Assets.ContractVariant min|max|alt`, `pw.Assets.CostProxy`.
- **Reverting one ID:** set `art.enabled` to false. Nothing is deleted.
- **Enforcement:** a validator rule fails the build on any hard reference to `/Game/Art/` or `/Game/Generated/` outside the resolver.

## 6. Integration route A (moved)

Moved to `ASSET_INTEGRATION.md` to keep this file under 500 lines. Section numbers are unchanged there.

## 10. Audio

Audio follows the same principle. Gameplay plays `sfx.*`, `amb.*`, `mus.*` and `vo.*` IDs through the same resolver, which falls back from real, to synthesized, to silence with a log line.

- **Registry entries** carry an `audio` block: kind (`one_shot`, `loop`, `music`, `dialogue`), attenuation (inner and falloff distance in metres), concurrency, a loudness target (dialogue −23 LUFS ±2) and a `placeholder` synth spec.
- **Placeholders** all come from one parameterized MetaSound, so no files are generated:
  - UI beeps: confirm is an 880 Hz square wave for 80 ms, cancel 440 Hz;
  - footsteps: 60 ms of filtered noise pitched by surface material;
  - ambiences: shaped noise loops (rain is high-passed white noise);
  - music: silent;
  - voice: silent, with subtitles and viseme timing of 70 ms per character (minimum 1.2 s).
- **The Gate alert placeholder** is a distinct three-tone pattern that resembles no real emergency tone.
- **Real audio** arrives by route A or B as 48 kHz, 16- or 24-bit WAV. The same provenance rules apply; in addition there is no voice cloning of real people, and library licences are recorded.

## 11. Source control

| Committed | Ignored |
|---|---|
| registry, contract, mapping, expression and provenance JSON in `Data/` | `Content/Fab/` (already ignored) |
| palette and tile PNGs in `Art/Blocks/` (a few KB each, plain git) | `Content/Generated/` (baked blocks, texture arrays) |
| block master materials and UI frames (a few `.uasset`, LFS) | `Content/_Staging/`, `Art/Incoming/`, `Art/Imported/` |
| thin levels (`.umap`, LFS), with layouts preferably kept as data | `Saved/Reports/` |
| route A test fixtures (under 5 MB in total) | editable sources: `.blend`, raw AI outputs, high-resolution sources |

`.gitignore` does not yet list `Content/Generated/`, `Content/_Staging/`, `Art/Incoming/` or `Art/Imported/`, and `.gitattributes` does not send `*.glb`, `*.blend`, `*.psd` or `*.ogg` to LFS. Add these before M1; this plan changes neither file.

**LFS policy.**

- **Quota.** The working figure is 1 GB of free LFS storage on the private repository. Verify it on GitHub's billing page, because GitHub has changed LFS pricing before.
- **Why be strict.** LFS keeps every version of every file, and deleting a file does not free quota; purging LFS objects requires recreating the repository. Nothing binary is ever committed "to try it".
- **Through M8.** The block pipeline is text and tiny PNGs. LFS use stays under 250 MB, measured at every milestone gate with `git lfs ls-files -s`.
- **Real art will not fit.** It is estimated at 2–5 GB imported and far more as source files. At the start of M8 the owner chooses between paid GitHub LFS data packs, a separate art repository on another host, or a binary-oriented version-control system. Source files stay on backed-up local storage, with their hashes recorded in provenance.

**The CC0 packs.** `Content/Fab/Downloads/` holds two unextracted zips: the Kenney Furniture Kit (5.1 MB) and OpenGameArt's Ultimate House Interior pack (12.8 MB). Nothing references them, they depict Western furniture, and both can be downloaded again from the URLs in `ART_PROVENANCE.md`. **Recommendation: delete them** (or move them outside the project folder if wanted as dimension references). Keep the `Content/Fab/` ignore entry and record the removal in `ART_PROVENANCE.md`.

## 12. What gets built when

| Milestone | Pipeline work | Evidence at the gate |
|---|---|---|
| M0 | none; the owner confirms target hardware, budgets and the decisions below | — |
| M1 | palette and about 40 tiles; texel classes and texture group; block materials with face shading; AA comparison; registry schema, portable validator, CTest; contracts for M1 families; baker, runtime builder, resolver, error cube; `APWAssetActor`; skeleton freeze; block player (all 96 combinations) with procedural animation and first-person arms; grips needed for the gate; lighting rig; budgets and first cost proxies; coverage v0 | in both cameras: sit, read, select a small item, use a tool, speak to someone, pass through a furnished room, switch camera; zero hard art references; budgets recorded with blocks and proxies |
| M2 | anchors, seats and sockets used by interaction; full grip set, held items, fp/tp pairs; expressions, visemes, look-at; NPCs from AppearanceRecord; identity kits for the first cast; rendered-vs-collision test; contract variants; placeholder audio; UI icon generator; interior test suite | contact scenarios pass at the proportion extremes |
| M3 | weather and night lighting; festival and religious block assets through cultural review; all six origin homes; crowd merging; `tone.*` scenes and captures; child proportions profile | identity test: 7 of 8 at 12 m |
| M4 | Gate kit and reserved ramps; a distinct material family for the virtual tower; route A MVP with fixtures; provenance schema and validator | fixtures import, fail and report correctly |
| M5 | route A complete; route B; the invariance harness (gameplay hashes, golden saves, collision fingerprint); elderly posture presets for later life | harness green on block-versus-proxy runs |
| M6 | crowd-scale budgets; cost-proxy runs at crowd density | benchmark route within budget with proxies |
| M7 | palette extensions for foreign locations, from the 6 free ramps | — |
| M8 | pipeline freeze; art storage decision; per-ID art briefs generated from the registry (dimensions, contract, block captures, anchors); coverage by milestone | M9 entry: every ID has a contract and block; baselines recorded |
| M9 | waves; three tests per wave; the presentation-adjustment period | (a), (b) and (c) in §9 |
| M10 | ship-blocked gate; store AI disclosure; blocks stripped from the cook | release candidate |

**Decisions for the owner:**

1. Target hardware and the budget table in §4.
2. Procedural placeholder animation, or Epic's free mannequin animations from M1.
3. Gaps in the AppearanceRecord: schema v1 has no lower-garment choice (trousers, skirt, salwar-kameez), no casual-wear colour and no height.
4. Forward rendering with MSAA, or deferred with TSR (settled by the M1 capture).
5. The CC0 packs: delete (recommended) or move out of the project.
6. Where real art is stored at M9 (decide at M8).
7. Whether a three-asset rehearsal import on a throwaway branch at M5 fits "real art only after the game is complete".
