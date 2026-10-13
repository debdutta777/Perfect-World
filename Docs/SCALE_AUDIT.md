# Scale audit of the Kolkata lane (M1-13, 2026-10-12; circulation and datum pass, 2026-10-13)

Scope: `PerfectWorldLaneBuildings.cpp`, `PerfectWorldLaneShops.cpp`, `PerfectWorldLaneProps.cpp` and the shared `PerfectWorldLaneBlocks`/`PerfectWorldLaneBarriers` files, checked against the reference dimensions in `ASSET_PIPELINE.md` (doors 90 x 210; windows 120 x 135 with the sill at 75; 45 cm chhajja; storeys 3.0 m; railing 100; stair riser 17.5) and the vehicle, stall and pole sizes given for the task. Units are centimetres. Heights in the lane code are measured from the road top (`GroundZ`); the footpath top is 15, so "above the footpath" means value minus 15. The walking player is a 68 cm wide, 165 cm tall capsule that steps up 45.

Status: **implemented, nothing compiled or run** (Rung 0). The circulation and datum pass in section 2 needs the integrator's Unreal build, the `Lane footpath route` log line (section 4) and a walk in PIE in both cameras. Line numbers are in the files as changed.

## 1. First pass (M1-13): dimensions that stand

| Item | Value now | Reference |
|---|---|---|
| Rooftop stair-room door | 90 x 210, room 235 high | door 90 x 210 |
| Balcony door leaf | 90 x 210, frame 104 x 217 | door 90 x 210 |
| Residence door leaf | 90 x 210, frame 104 x 217 (see 2.3 for the threshold) | door 90 x 210 |
| Upper-floor window | opening 120 x 135, sill 75 above the room floor, 8 thick sill slab, glass 106 x 121, 45 deep chhajja | window 120 x 135, sill 75 |
| Second-row window | frame 120 x 135, sill 75 | window 120 x 135 |
| Ground-floor residence window | opening 120 x 135, sill 75 above the footpath, 6 bars, 45 cm chhajja | same |
| Window air-conditioner | 60 x 45, hangs under the sill | about 60 x 40 |
| Balcony railing and roof parapet | top at 100 | railing 100 |
| Shutter bay | clear opening 240 above the footpath, drum box to 270 | shutter 240-270 |
| Stall counter | top 90 above the footpath | about 90 |
| Footpath | top 15 above the road, kerb stone 17 | kerb 15-20 |

Descriptions corrected in this pass (the earlier text was wrong or loose):

| Item | Earlier description | What the geometry actually was |
|---|---|---|
| Water tank | "130 across, 150 tall on a 40 base" | A 130 square body 110 high, plus a second copy turned 45 degrees that made the horizontal envelope about 184 across, on a 160 square base 40 high, with a lid reaching 120 above the base. Rebuilt in 2.6. |
| Taxi | "435 x 165 x 155", then "160 high" | 436 long, 165 wide; the body roof stood at 146 and the roof sign took the total to 160, so "160 high" included the sign. Rebuilt in 2.6 with the roof at 155 and the sign as a separate accessory. |
| Handcart | "deck 180 x 90, fine" | The deck is 180 x 90 with its top at 60. Overall, with handles and wheels, the cart is 240 long (handles reach 60 past each deck end), 104 wide and 92 high with its load. |
| Pot plant | "pot 32, plant 78 tall" | Pot 32 high, leaf block to 78, and a turned top block that reaches 84 above the footpath (Z 99). |
| Utility pole | "900 tall" | 900 above the road top (`GroundZ`), so about 885 exposed above the footpath. The reference datum is the road. Wires at 849 from the road. |
| Scooter | "Activa class, 190 x 70 x 112, 12 inch wheels" | 190 x 68 x 112, wheelbase 140, 50 cm tyres. "12 inch" is a rim class; 50 cm is the tyre's outer diameter. |
| Window "frame" | an opening | A filled dark rectangle 120 x 135 with the glass 106 x 121 inset. There is no hole in the wall; the structural opening is documented as 120 x 135 and the glazing as 106 x 121. |
| "75 cm plinth" | a raised floor | A decorative fascia from 15 to 75 (60 tall) in front of the wall. It is not a raised interior floor and must not be read as one. |

## 2. Circulation and datum pass (2026-10-13)

| # | Item | File:line (after) | Before | After |
|---|---|---|---|---|
| 2.1 | Stall counters on the footpath | Shops.cpp:99-134, Buildings.cpp:168-189 | Counter, crates and shelves stood 34-92 cm out from a 150 cm footpath, leaving **58** | Each open stall is a 100 deep niche cut into the building (walls split in `AddWallVolume`); counter front 8 cm behind the building line. Nothing of a stall stands on the footpath |
| 2.1 | Awning poles | Shops.cpp:170-188 | Two poles at the awning edge, 119 cm from the wall on a 150 footpath | Poles only where the awning is 165 deep (setback 60 or 120): 159 cm from the wall, so a 159 cm wall-side route remains behind them. Zero-setback awnings (125 deep) have two stay rods above 230 and no poles |
| 2.1 | Utility poles | Props.cpp:12, 28-41 | Centre at 268, collision out to 284, leaving 116 beside a zero-setback front | Centre 264, collision half width 15: out to 279, leaving **121** |
| 2.1 | Street trees | Props.cpp:13, 115-124 | Centre at 335 in the middle of the footpath, so 50-70 cm each side | Centre 266 on the kerb edge (collision 253-279), leaving **121**; the canopy no longer pokes through the wall |
| 2.1 | Pots | Shops.cpp:19, 83-95, 270-290 | 0-2 per building anywhere on the frontage, 48 deep | Placed last: only on footpaths with a setback of 60 or 120, never within 60 of a stall or 90 of a door; leave at least 129 beside them |
| 2.1 | Gaps between buildings | Barriers.cpp:20-26 | No footpath in a gap, the barrier sank 10 cm into a setback neighbour's footpath | The footpath and kerb carry across gaps of 450 or less; the barrier abuts the neighbouring walls exactly |
| 2.2 | Awning headroom | Shops.cpp:13-17, 160-169 | Valance 206-230 (191 above the footpath), slope edge 228 | Valance **225-249** (210 above the footpath), slope edge 247 (the 4 cm slab's underside there is about 245, 230 above the footpath), wall attachment 285 |
| 2.3 | Residence door and step | Shops.cpp:196-205 | Leaf from 15, step top at 28: 197 clear above the step | Entrance floor stays at the footpath top (+15); the step is a threshold plate 0 to 15.5 (5 mm proud so it does not fight the footpath). Clear opening 90 x 210 above the floor |
| 2.4 | Floor datum | Blocks.h:22-30, Buildings.h:23-45, Buildings.cpp passim | Ground floor at 15, floor n at n x 300 (285 between the ground and first floors) | `FloorZ(n) = 15 + n x StoreyHeightCm`; per-building `StoreyHeightCm` (300 default, `OlderStoreyHeight` 360 available, none authored yet). Windows (sill 75 above the room floor), balconies, bands, stains, parapet and roof derive from it; every upper level rises 15 |
| 2.5 | Collision roles | Blocks.h:100-110 and 146-156, Blocks.cpp:17-80 and 149-215 | Colour table had a boolean; pole, trunk, taxi, scooter, handcart, counter and pot used Pawn-only barriers | Table rows name an `ELaneCollision` role. New `Prop` role (invisible `PropBlock`) blocks Pawn, **Camera and Visibility**; `Boundary` (invisible `Barrier`) stays Pawn only. Boxes follow the visible parts to within 5 cm |
| 2.6 | Roof tank | Buildings.cpp:85-95 | 160 base, 130 body 110 high, turned duplicate, 70 lid | 130 base 40 high, **110 x 110 x 110** body, 70 x 70 x 10 lid: 120 above the base; duplicate removed |
| 2.6 | Scooter | Props.cpp:156-182 | 190 x 68 x 112, axles at +-70, handlebar 68 wide | **183 x 69 x 113**, 50 cm tyres on axles +-66.5 (133 wheelbase), handlebar 69 wide to 113; comment corrected |
| 2.6 | Taxi | Props.cpp:127-153 | 436 long, roof 146, sign to 160 | **435 long** (bumpers +-217.5), 165 wide, glazing to 147, roof 147-155, the sign a separate accessory 155-165 |
| 2.6 | Handcart | Props.cpp:184-209 | One box 242 x 104 x 95 | Five boxes for deck, wheels and handles, plus one per crate; the visible shape is unchanged |

### Collision after this pass

| Role | Blocks | Used for |
|---|---|---|
| World (visible, coloured) | Pawn, Camera, Visibility | `WallOchre` to `WallDustyBlue`, `Asphalt`, `Footpath`, `Ground` |
| Prop (`PropBlock`, invisible) | Pawn, Camera, Visibility | utility poles (full 900 height), tree trunks (330), taxi (body to 85, cabin to 155, sign), scooters (body, seat, column, bar, two tyres), handcart (deck, wheels, handles, crates), stall counters and crates, pots, awning poles |
| Boundary (`Barrier`, invisible) | Pawn only | gaps between first-row buildings, side alley ends at |Y| 1100 |
| None | nothing | trim, glass, shutters, doors, signs, goods, awnings, leaves and canopies, wires, concrete detail |

Known limits of the prop boxes: the taxi body box is filled to the ground between the wheels (nothing visible there but the chassis); tree canopies and pot foliage above the pot block are not solid to the camera.

## 3. How the 120 cm route is guaranteed

Cross-section along a footpath, measured as |Y| from the road centre: the kerb is at 250, a zero-setback front is at 400, a setback front at 460 or 520.

* Everything the player can bump into on a footpath is either on the kerb edge, with collision ending at |Y| 279 (utility poles 249-279, tree trunks 253-279), or against the wall. Counters, shelves and goods are inside the building. So the strip from 279 to the front is clear of kerb furniture: **121 cm at zero setback**, 181 and 241 at setback 60 and 120.
* The only collidable things on the wall side are pots, awning poles and the doorstep. The doorstep is flush with the footpath. Awning poles stand 159 cm or more from the wall, leaving a wall-side route of at least 159. Pots exist only on setback 60 and 120 footpaths and keep 129 (setback 60) or 189 (setback 120) beside them, and never stand in front of a stall or door.
* Where a utility pole or tree meets a pot or awning pole at the same X, the free width is still at least 121 on one side (checked by the cases above).
* Gaps between buildings carry the footpath across, and the barrier no longer sinks into a setback neighbour's footpath, so the route is unbroken along the whole row. Side alleys cut the footpath and are crossed on the road (a 15 cm step down).
* The two end buildings across the lane have recessed counters, no awning poles (setback 0) and no pots, and no pole or tree stands on their footpath.
* Passing room: a 150 footpath at 121 clear at its narrowest holds one person walking and one stepping aside; setback 60 (210) and 120 (270) footpaths have room for two to pass.

`ReportFootpathRoutes` (`PerfectWorldLaneRoutes.cpp`, compiled out of shipping builds, called at the end of `BuildKolkataLane`) tests this on the generated layout. It samples both footpaths every 5 cm, subtracts every invisible blocking box in the 60-180 cm walking band, requires an interval of at least 120 each step and an overlap of at least 68 with an interval from the step before, and logs `Lane footpath route: continuous; narrowest clear width ...` or a Warning with the number of breaks and the worst X. **Not yet run.**

## 4. Checked and left as is

| Item | Value | Reference | Verdict |
|---|---|---|---|
| Storey height | 300 (flats) | 300 flats | keep; 360 is available per building for authored older houses and the school |
| Footpath height | 15 | kerb 15-20 | fine; the player steps up it (max step 45) |
| Balcony slab | 176 wide, 100 deep, 14 thick | no contract | fine, but decorative (see 5) |
| Shop signboard | Z 272-298 | no contract | fine |
| Street tree canopy | from 300 to 560 | head clearance | fine |
| Wires | 2-2.5 thick | utility lines 2.5 | fine |
| Far-row glass strips (`BuildLow`) | 160 x 120 per bay | skyline at 40 m or more | deliberate abstraction; not a window |
| Auto-rickshaw, bicycle | not in the lane | 265 x 140 x 170; wheel 70 | nothing to fix; use these when added |

## 5. Remaining, tracked work (not fixed here)

1. **Solid building volumes and interiors.** Every near building is still a solid wall-coloured box (`AddWallVolume`), now only notched for stall niches. Interiors need 25 cm shell walls, real openings, floors and ceilings, with the distant massing left solid (`LANE_LIFE_PLAN.md` C, `LANE_LIFE_IMPLEMENTATION.md` stage 3).
2. **Door and window frames** are filled decorative rectangles; separate jambs, head and sill around a measured 90 x 210 (or 120 x 135) aperture arrive with the shells.
3. **Balcony and roof collision.** Balcony slabs and rails have no collision, and the rear and side parapets (90) differ from the front coping (100). Before any balcony is reachable: a walkable slab plus matching rail collision and one authored protective height.
4. **Navigation geometry.** Navigation is disabled on every lane component (`SetCanEverAffectNavigation(false)`); NPC routes need explicit walkable geometry or authored waypoints.
5. **Alley ends** are invisible Pawn barriers; they need visible architecture or gates.
6. **Tree canopy and pot foliage** have no camera collision.
7. **Registered asset data** that disagrees with the lane (`wooden_panel.json` and `main_grill.json` leaf 80 x 200, `louvred_shutter.json` aperture, `tea_stall_counter.json` worktop 95): stage 1.5 and 1.6 of the implementation plan.
8. **Verification.** Build, read the route log, walk both cameras past a stall, a pole, a tree and a doorstep, and capture awning headroom and the doorstep in a packaged build (stage 1.7).
9. **Running speed** is 4 m/s against the 3.5 m/s procedural reference: a tuning decision, not a scale error (player files not touched here).
