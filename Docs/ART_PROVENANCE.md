# Concept art provenance

Generated with the built-in Imagegen tool on 19 September 2026. All subjects are original fictional people and scenes. These are visual development references, not ready-to-use meshes or playable environments.

| Delivered file | Purpose | Generation source |
|---|---|---|
| Art/kolkata-nabapara.png | Canonical setting direction: fictional para within Kolkata, school commute, river warehouse Gate | exec-699b4232-7392-4e00-acc9-57885ec75e11.png |
| Art/student-lineup.png | Age-16 student silhouette and outfit exploration | exec-695537ab-8987-4b9b-bf17-abd850f64dc3.png |

Kolkata prompt summary: wide stylized3D isometric production concept, Nabapara fictional neighborhood, shuttered older houses, balconies, courtyard secondary school, books/stationery, tea in clay cups, yellow taxi, monsoon puddles, river warehouse containing a teal-violet Gate. Distant river and bridge are atmospheric references, not a geographically reliable sightline. Generated decorative religious imagery and tiny signage are not approved environment assets and require art review.

Student prompt summary: four modestly dressed Indian16-year-old school students, varied skin tones, pale-blue/navy uniform directions, bags/notebooks, practical shoes, expression studies, clean separated full figures. This sheet was generated before the Kolkata update. Its faint background buildings are **noncanonical** and must not guide Kolkata environment production; only student costume/silhouette exploration is retained. Uniforms are proposed fictional school designs, not a statement about an actual institution.

An earlier western-Deccan environment draft was superseded by the user's West Bengal instruction and is intentionally excluded from the delivered art folder. No image is a representation of a completed in-engine scene.

## Owner-supplied reference sheet — 20 September 2026

The owner supplied a new reference character sheet on 2026-09-20 showing anime-styled school students in navy/light-blue uniforms. This sheet is usable for **character style reference only** — uniform colour direction, anime-styled proportions and rendering, and the general costume/silhouette approach for the character editor described in CHARACTER_CREATION.md and ART_DIRECTION.md.

Its backdrop shows a South Indian gopuram temple and a hill fort. This background is **noncanonical**: it is not the game's Kolkata / West Bengal setting and must not guide environment, temple, or fort production. This follows the same handling already applied to `student-lineup.png` above, whose faint background buildings are likewise flagged noncanonical — only the foreground character/costume study is retained from either sheet.

## Downloaded placeholder assets — 20 September 2026

Two CC0 asset packs were downloaded for greyboxing the six starting rooms. Both are public domain, usable commercially, and require no attribution. They live in `Content/Fab/Downloads/`, which `.gitignore` excludes, so they are not committed and consume no Git LFS quota. Anyone cloning the repository re-downloads them from the URLs below.

| Pack | Source | Licence | Size | Contents verified |
|---|---|---|---|---|
| Kenney Furniture Kit 2.0 | `https://kenney.nl/media/pages/assets/furniture-kit/440e0608a4-1677580847/kenney_furniture-kit.zip` | CC0 1.0, commercial use allowed, credit optional | 5.1 MB | 140 models in OBJ, FBX, GLB and STL. Confirmed present by listing the archive: `bedSingle`, `bedBunk`, `bedDouble`, `desk`, `deskCorner`, `chairDesk`, `bookcaseClosedDoors`, `ceilingFan`, `doorway`, `doorwayOpen`, `wall`, `wallCorner`, `floorFull`, `floorHalf`, lamps, side tables and benches. |
| Ultimate House Interior Pack (June 2020) | `https://opengameart.org/sites/default/files/ultimate_house_interior_pack_-_june_2020.zip` | CC0 | 12.8 MB | 123 models in FBX, OBJ and Blend. Confirmed present: nine door variants and a bookshelf, covering the door and window variety the Kenney kit is thinner on. |

Licence text for the Kenney pack is inside the archive as `License.txt` and was read directly rather than assumed; it states the content is "free to use in personal, educational and commercial projects" and that crediting Kenney "is not mandatory".

No footlocker or trunk mesh was found in either pack. The children's home dormitory needs one, so it is either a substitute (a drawer cabinet or crate) or the first bespoke mesh to author.

These are **placeholder greybox props, not final art**. They are Western-styled low-poly furniture and do not depict a Kolkata home. They exist to prove the room-loading and traversal systems before any setting-accurate art is commissioned, and every one of them is expected to be replaced. Do not treat their look as art direction; ART_DIRECTION.md remains authoritative.
