# World scale

**Document status:** plan, 2026-09-28. The scale is the owner's decision (`GAME_PLAN.md` §10): many real world cities as block cities, fictional places, and a dungeon realm ten times bigger than Earth's total surface. The **build approach** below is Claude's proposal, not yet reviewed by GPT-6: Codex is not installed on the current cloud PC, so the critique `AGENTS.md` asks for has not run. **Nothing here is implemented.** Where this document and `GAME_PLAN.md` disagree, `GAME_PLAN.md` wins. Nations, rankings and Country Level stay in `GLOBAL_WORLD.md`; the block style, asset IDs and budgets stay in `ASSET_PIPELINE.md`.

---

## 1. The one rule of scale

**Nothing large is built by hand.** A hand-authored place is a *layout* of asset IDs (`ASSET_PIPELINE.md`). Anything bigger than a few city blocks is *generated* from a seed and rules, the same way every time, and only what the player changed is saved. This is what makes a realm of 5.1 billion km² possible on one machine and in a save under the `SYSTEMS.md` budget.

Three kinds of place follow from that rule:

| Kind | Built as | Saved as |
|---|---|---|
| Authored place (the home, the school, a hero district) | A layout of asset IDs, placed by hand or by an agent from data | Nothing but world flags and object state by stable ID |
| Generated district (most of every city) | District archetype + seed → block massing, streets, interiors on demand | Only changed objects, keyed by district and stable ID |
| Deep Realm terrain | Realm seed + chunk coordinate → terrain, features, encounters | Only visited-and-changed chunk deltas |

Real art (M9) replaces the block meshes behind asset IDs. It never changes the generator, collision data or saves.

---

## 2. Real world cities

- **A city is data.** Each city has a stable ID, its nation (`GLOBAL_WORLD.md` §8 roster), its real name, and a list of districts. A district is either **authored** (a layout) or **generated** (an archetype plus a seed).
- **One hero district per city is authored.** The rest come from about a dozen district archetypes: old dense core, apartment belt, market, riverfront, rail yard, industrial, campus, government quarter, suburb, informal settlement, port, park. Each archetype defines street grammar, block heights, building kinds and the interiors that can appear.
- **Real, but not a replica.** Street layout, landmarks and skyline are recognisable block approximations. Real people never appear (`GLOBAL_WORLD.md` §8). Institutions, companies and incidents stay fictional.
- **Travel between cities is a journey, not a flight over a modelled planet.** Rail, road and air are scenes with a declared game-time cost under the time contract (`GAME_PLAN.md` §5). The Earth's surface between cities is not modelled.
- **Order:** Kolkata (Nabapara first) → the rest of Kolkata → the Indian cities the story needs → foreign cities in the order the Convergence and the story need them. The owner confirms the list (W1).

## 3. Fictional places

Fictional places use the same city and district data, marked `fictional: true`. Examples: towns that exist only in this alternate Earth, Gate-response academies, sealed Gate sites, a hidden district under a real city. Nabapara and Nabapara Vidyalaya are already fictional places inside real Kolkata. A fictional place may sit inside a real city, between cities, or inside the Deep Realm.

---

## 4. The Deep Realm (the dungeon)

### 4.1 Size

Earth's surface is about 510 million km². The realm is **at least ten times that: 5.1 billion km²**, laid out as a square roughly **71,400 km on a side**. At 1 m blocks in 32 m × 32 m chunks, that is about 5 × 10¹² chunks. That can never be stored, so it is never stored: every chunk can be regenerated exactly from the realm seed and its coordinate.

### 4.2 Coordinates

- The portable core never stores a realm position as a single float. A position is a **64-bit integer chunk coordinate plus a local offset in metres** inside the chunk (`Core/RealmCoordinate`).
- Unreal only ever sees a local window around the player. When the player crosses a sector boundary the world is **re-origined**, so rendering precision is always centimetre-accurate. Unreal 5.8's large-world coordinates would reach far enough, but the design does not depend on them.
- Distances, directions and travel times are computed in the core with integer chunks, so they are identical on every machine.

### 4.3 Generation

- **Counter-based, not sequential.** Each chunk is generated from `hash(realmSeed, chunkX, chunkY, layer)`, so any chunk can be built alone, in any order, on any thread, with the same result. This is separate from the life's RNG streams (`SYSTEMS.md`), which it never advances.
- **Layers of meaning above the terrain:** regions (thousands of km, a theme and a danger rank F to S), sectors (tens of km, a re-origin cell and a travel node), and chunks (32 m, the unit of building and saving). Danger generally rises with distance from the Gate anchors that connect the realm to Earth.
- **Landmarks and dungeons inside the dungeon** (ruins, towers, lairs, settlements of other explorers) are placed by a sparse hashed grid per region, so they exist before anyone visits them and never move.

### 4.4 Travel and time

Nobody walks 71,000 km. The realm is crossed by Gate anchors, discovered waypoints, realm transit and expedition vehicles. Time inside runs on the **mission clock** (`GAME_PLAN.md` §5): the outside world advances at a capped rate, so an expedition costs weeks of a life, not years.

### 4.5 Saving

Only chunks the player **changed** are saved, as deltas keyed by chunk coordinate: a block removed, a chest opened, a camp built. A visited-but-unchanged chunk saves nothing. Deltas have their own budget inside the `SYSTEMS.md` save budget, and a cap with a documented rule for which old deltas revert.

### 4.6 It stays optional

The realm is supernatural content. A player who never enters a Gate never needs it (`GAME_PLAN.md` §1). Its existence still shapes the world: news, rankings, insurance, Country Level.

---

## 5. Streaming and performance

- Chunks and generated districts are built as **instanced block meshes** on worker threads and streamed in a radius around the player. This uses a project streamer, not World Partition, which stays out of the baseline (`ROADMAP.md` §1.4).
- Collision and navigation come from the same generated data, never from the meshes.
- Every streamer respects the `ASSET_PIPELINE.md` §4 budgets. The first measurement is part of the lane below.

---

## 6. Build lane

The steps are in `ROADMAP.md` §5a (M1-25 to M1-31). The portable steps need no editor and start now:

1. `Core/RealmCoordinate` — the integer-chunk position, its arithmetic, and the exact realm extents.
2. `Core/RealmGenerator` — the counter-based chunk generator with golden values.
3. `Core/ChunkDeltaStore` — sparse, versioned chunk deltas with a save round trip.
4. `Game/RealmStreamer` — instanced-block chunks around the player, with re-origin.
5. `Core/CityData` + district archetypes — city records and a generated-district builder.
6. The Kolkata hero district (Nabapara) with one generated neighbouring district.
7. A synthetic-load measurement against the budgets.

## 7. Open owner questions

| ID | Question |
|---|---|
| **W1** | Which real cities, in what order, after Kolkata? |
| **W2** | May real street layouts come from OpenStreetMap? It is free but requires ODbL attribution in the credits. The alternative is hand-approximated layouts |
| **W3** | Is there one Deep Realm shared by every nation, or does each nation's Gates lead to its own region of it? |
| **W4** | The realm's look: one continuous surface, or stacked layers (floors) going down like a true dungeon? |
