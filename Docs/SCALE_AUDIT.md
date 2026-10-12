# Scale audit of the Kolkata lane (M1-13, 2026-10-12)

Scope: `PerfectWorldLaneBuildings.cpp`, `PerfectWorldLaneShops.cpp`, `PerfectWorldLaneProps.cpp` checked against the reference dimensions in `ASSET_PIPELINE.md` (doors 90 x 210; windows 120 x 135 with the sill at 75; 45 cm chhajja; storeys 3.0 m; railing 100; stair riser 17.5) and the vehicle, stall and pole sizes given for the task. Units are centimetres. All "after" line numbers are in the files as changed. Heights in Shops.cpp are measured from the road top (`GroundZ`); the footpath top is 15, so "above the footpath" means value minus 15. Nothing was compiled or run (Rung 0): the change set still needs the integrator's Rung 2 build and a look in PIE.

## Changed

| Item | File:line (after) | Before | After | Reference |
|---|---|---|---|---|
| Rooftop stair-room door | Buildings.cpp:110 | 70 x 175 | 90 x 210 (room is 235 high, so the lintel is 25) | door 90 x 210 |
| Balcony door (leaf) | Buildings.cpp:41-43 | 88 x 224, frame 104 x 232 | leaf 90 x 210, frame 104 x 217; glass lite 78 high, lower | door 90 x 210 |
| Residence street door (leaf) | Shops.cpp:144-147 | 80 x 211, frame 100 x 223 | leaf 90 x 210, frame 104 x 217; glass lite 56 x 60 | door 90 x 210 |
| Upper-floor window | Buildings.cpp:20-37 | frame 110 x 150, sill at 85, glass 92 x 132, no sunshade | opening 120 x 135, sill at 75, sill slab 8 thick, glass inset 7 cm, shutters 36 wide each side, 45 cm deep chhajja 6 thick above | window 120 x 135, sill 75, chhajja 45 |
| Second-row window (distant) | Buildings.cpp:210-212 | frame 120 x 150, sill at 80, glass 96 x 130 | frame 120 x 135, sill at 75, glass 106 x 121 | window 120 x 135, sill 75 |
| Ground-floor residence window | Shops.cpp:161-174 | frame 116 x 130, sill at 95 (80 above the footpath), 5 bars | opening 120 x 135, sill 75 above the footpath (Z 90), 6 bars, 45 cm chhajja | window 120 x 135, sill 75, chhajja 45 |
| Window air-conditioner | Buildings.cpp:148 | 60 x 45 at Z 30-75 (touched the old sill) | same size, Z 22-67 so it hangs under the new sill | typical window unit, about 60 x 40 |
| Balcony railing | Buildings.cpp:46-55 | top rail at 104, balusters 96 | top rail at 100 (92-100), balusters 92 | railing 100 |
| Hanging washing (balcony) | Buildings.cpp:67-68 | top at 102 | top at 100 | keeps to the rail |
| Roof parapet | Buildings.cpp:80-84 | 75 wall plus 10 coping = 85 | 90 wall plus 10 coping = 100 | railing 100 |
| Shutter bay (roller shutter) | Shops.cpp:10, 43-48 | panel to 252 (237 clear), drum 250-268 | panel to 255 (240 clear), drum 253-270, slats follow | shutter height 240-270 |
| Stall counter | Shops.cpp:11, 90-98 | top at 75 (60 above the footpath), crates 75-95 | top at 105 (90 above the footpath), cloth 5 thick, crates from 105 | counter about 90 high |
| Utility pole | Props.cpp:8, 28-37 | 715 tall, cross-arm at 640, wires at 664 | 900 tall, cross-arm at 825, arms to 851, wires at 849; transformer box lifted from 470-570 to 590-690 | poles 900+ |
| Taxi (Ambassador-like) | Props.cpp:125-144 | body 440 + bumpers = 464 long, 174 wide (wheels out at 204), 160 high | 436 long, 165 wide, 160 high; wheels 62 tall on a 236 wheelbase, tucked inside the body | 435 x 165 x 155 |
| Scooter | Props.cpp:147-157 | 160 long, wheels 40 tall | 190 long, wheels 50 tall (12 inch), 70 wide at the handlebar, 112 high | Activa class, about 183 x 69 x 113 |

## Checked and left as is

| Item | File:line | Value | Reference | Verdict |
|---|---|---|---|---|
| Storey height | Buildings.cpp `Storey` | 300 | 300 flats | keep, as asked |
| Footpath height | LaneBlocks.h `FootpathHeight` | 15 | kerb 15-20, riser 17.5 | fine; the player steps up it (max step 45) |
| Door step | Shops.cpp:148 (concrete, 13 high, 42 deep) | riser 13, tread 42 | riser 17.5, tread 25 | acceptable for a single step |
| Balcony slab | Buildings.cpp:47 | 176 wide, 100 deep, 14 thick | no contract | fine |
| Awning and valance | Shops.cpp awning block | lowest edge 191 above the footpath | head 165 | clear for the 165 cm player |
| Shop signboard | Shops.cpp:24 | Z 272-298 | no contract | fine |
| Pot plants | Shops.cpp:55-59 | pot 32, plant 78 tall | no contract | fine |
| Water tank | Buildings.cpp:94-97 | 130 across, 150 tall on a 40 base | 1000 L tank about 110 x 120 | slightly large, left for the roofline |
| Handcart | Props.cpp:160-173 | deck 180 x 90 at 45-60, wheels 60 | thela about 180 x 90 | fine |
| Street tree | Props.cpp:112-123 | trunk 26 square, canopy from 300 to 560 | head clearance | fine |
| Wires | Props.cpp | 2-2.5 thick | utility lines 2.5 | fine |
| Far-row glass strips (`BuildLow`) | Buildings.cpp:220 | 160 x 120 per bay | skyline, seen at 40 m or more | deliberate abstraction; not a window |
| Auto-rickshaw, bicycle | not in the lane | none | 265 x 140 x 170; wheel 70 | nothing to fix; use these when they are added |

## Collision split (for reference, M1-13)

Solid and blocking everything (camera boom included): the wall colours (`WallOchre` to `WallDustyBlue`), `Asphalt`, `Footpath`, `Ground`. Everything else on the colour table has no collision (trim, glass, shutters, doors, signs, goods, awnings, leaves, wires, concrete detail). Pole, tree trunk, taxi, scooter, handcart, stall counter and pot fences are invisible `Barrier` boxes (`ELaneColour::Barrier`) that block the Pawn channel only, so the third-person camera passes through them. `PerfectWorldLaneBarriers.cpp` fills the gaps between first-row buildings and shuts each side alley at |Y| 1100.
