# Asset integration

**Document status:** part of `ASSET_PIPELINE.md`, split out on 2026-09-23 for length. How real art gets in: both import routes, AI-generated asset provenance, coverage and the M9 art validation. `ASSET_PIPELINE.md` holds the block style, block characters, asset IDs, placeholder generation, the resolver, audio and source control.

## 6. Integration route A — drop folder

```
Art/Incoming/prop.furniture.bed_plank_single/   (Art/Incoming/ is git-ignored)
  manifest.json      required; written last, and its arrival triggers the import
  provenance.json    required unless origin is "owner_authored"
  export/            model.glb or model.fbx, optional _lod1 / _lod2 files
  textures/          PNG or TGA named <slot>_BaseColor, <slot>_Normal, <slot>_ORM
  source/            optional .blend or other editable source
  licence/           licence text or terms snapshot for third-party and AI sources
```

```json
{
  "manifest": "pw.incoming/1",
  "asset_id": "prop.furniture.bed_plank_single",
  "kind": "static_mesh", "origin": "ai_generated", "contract": "prop.bed@1",
  "files": {
    "mesh": "export/model.glb", "lods": ["export/model_lod1.glb"],
    "textures": { "frame_BaseColor": "textures/frame_BaseColor.png", "frame_Normal": "textures/frame_Normal.png",
                  "frame_ORM": "textures/frame_ORM.png", "bedding_BaseColor": "textures/bedding_BaseColor.png" }
  },
  "material_slots": ["frame", "bedding"],
  "forward_confirmed_by": "owner", "reviewer": "owner",
  "notes": "Retopologised and repainted in Blender."
}
```

**Import tool: a C++ commandlet**, `-run=PWImportArt`, called by a thin `Scripts/import_art.py` in the style of the existing build scripts. The script also runs fast file-level pre-checks and a `--watch` mode that polls every 10 s. Inside an open editor, `UPWArtInboxWatcher` (Directory Watcher) runs the same code. C++ rather than editor Python because:

- the validator is shared with route B, the baker and the portable registry;
- it is typed and unit-tested;
- it runs headless (`UnrealEditor-Cmd.exe PerfectWorld.uproject -run=PWImportArt -unattended`);
- it drives Interchange through `IAssetTools::ImportAssetTasks`.

Python would split validation across two languages. Unreal's built-in Auto Reimport stays off because it bypasses validation.

**Checks.** Each failure reports measured against expected, with a fix hint.

| Check | Rule |
|---|---|
| naming | folder name = `asset_id` = an existing registry ID; lowercase ASCII file names; the texture suffixes above |
| provenance | present and valid (§8); file hashes match |
| format | binary FBX 2020 or glTF 2.0 binary; PNG or TGA; no JPEG for Normal or ORM |
| scale | bounds inside the outer envelope and filling the inner; a 100×, 0.01× or 2.54× mismatch is reported as a unit error |
| pivot | props bottom-centre (min Z = 0 ± 1 cm, XY centre within 2 cm); architecture at its min corner |
| forward axis | front faces +X: fails if the footprint fits only after a 90° swap; a +X render goes into the report for the reviewer |
| triangle budget | per contract; LOD1 and LOD2 required above 5,000 triangles (generated at 50% / 20% if absent) |
| textures | power of two; ≤ contract maximum; Normal and ORM non-sRGB; median prop texel density 256–1,024 px/m |
| material slots | count ≤ contract; slot names match the contract's roles |
| mesh collision, sockets | `UCX_` collision and mesh sockets stripped with a warning: they live in data |
| rendered vs collision | the §3.2 ray comparison, run on the new mesh |
| characters | hierarchy and names equal `skeleton.humanoid@1`, extra bones only as leaves; joints within ±3%; ≤ 8 influences (4 on mobile LODs); ≤ 4 slots; every expression ID mapped to morph targets or face bones; the appearance-mapping diff passes |
| animations | skeleton match; notifies and duration per contract; no root motion unless the contract allows it |

**Failure.** The import runs into `/Game/_Staging/<id>/`, and only a complete pass moves it to `/Game/Art/...`. On any failure the staging copy is deleted, the registry is untouched, `_report.txt` and `_report.json` are written into the incoming folder, the editor shows a notification, and a headless run exits non-zero. Fix the files and re-save `manifest.json` to retry.

**Registry update.** Only the `art` block changes: status `real`, package paths, LODs, slots, input file hashes, importer version, route, date and reviewer. The importer hashes `dimensions_cm`, `envelope_cm` and `gameplay` before and after, and aborts if they differ. Provenance is copied to `Data/Provenance/<id>.json`, which is committed. The inputs move to `Art/Imported/<id>/<date>/`, a git-ignored local archive that is backed up externally.

## 7. Integration route B — editor import

1. Import through the Content Browser as usual (Interchange) into `/Game/_Staging/`.
2. Right-click → **Perfect World ▸ Assign to asset ID…** (a Content Browser extension in `PerfectWorldEditor`) opens a searchable registry picker, then a provenance form with the same fields as `provenance.json`.
3. The same `FPWArtValidator` runs and shows the same report in a dialog.
4. On a pass, the asset is moved and the registry and provenance record are written exactly as in route A, with `route: "editor"`.
5. **Guard rails.** A Data Validation rule (`UEditorValidatorBase`) flags three things: assets under `/Game/Art/` that no registry entry references; entries whose assets fail validation; and anything left in `/Game/_Staging/` for more than a day. A reimport reruns validation; if it fails, the entry becomes `real_invalid` and the resolver falls back to blocks.

## 8. AI-generated assets

Every AI-generated or AI-assisted asset carries a provenance record. It can be written by hand for route A or filled in through the form in route B:

```json
{
  "provenance": "pw.provenance/1", "asset_id": "prop.furniture.bed_plank_single", "origin": "ai_generated",
  "files": [ { "path": "export/model.glb", "sha256": "<hash>" }, { "path": "textures/frame_BaseColor.png", "sha256": "<hash>" } ],
  "generation": [ {
    "tool": "<product>", "tool_version": "<version>", "model": "<model>", "model_version": "<version>",
    "account_tier": "paid", "date": "2030-03-14T19:05:00+05:30", "seed": 81234,
    "prompt": "Low wooden single plank bed, four square legs, thin cotton mattress, plain, no text, no logos",
    "negative_prompt": "text, logo, watermark",
    "inputs": [ { "path": "source/block_capture_front.png", "origin": "own block capture of this ID" } ]
  } ],
  "licence": { "terms_url": "<tool terms URL>", "terms_snapshot": "licence/terms_2030-03-14.pdf",
               "commercial_use": "allowed", "output_ownership": "assigned_to_user", "indemnity": "none" },
  "human_edits": "Retopologised to 4,800 triangles; textures repainted; proportions matched to prop.bed@1.",
  "review": { "reviewer": "<person>", "date": "2030-03-15",
              "no_real_likeness": true, "no_trademarks": true, "no_named_style": true,
              "cultural_review": { "required": true, "reviewer": "<person>", "date": "2030-03-20", "result": "approved" } }
}
```

**Recommended workflow.** Generate from the ID's own block captures and contract (dimensions, silhouette cues), so outputs start at the right proportions.

**Rules.**

- **No real people's likenesses:** no real names in prompts, no photographs of real people as inputs.
- **No trademarks, logos, brand names or recognisable trade dress.** The taxi is a generic yellow cab, not a specific car model; phones and almirahs are unbranded; no real school's crest or uniform.
- **No style prompts naming artists or studios**, living or dead.
- **Cultural review** by a named human reviewer, with the owner signing off, for anything depicting Kolkata or Bengal specifically: religious places, objects and festivals (pandals, idols, shrines, symbols), food, dress and any script. Generators garble Bengali, so lettering in generated output is removed and replaced with authored text layers.
- **Religious imagery never enters enemy, loot or Gate kits** (`INDIA_WORLD_BIBLE.md`).
- **The reviewer is a person.** An AI agent may prepare a record, never approve one.

**Validation before import.** The schema is complete, file hashes match, a terms snapshot exists, and a reviewer and date are present. `cultural_review` is required whenever a `culture.*`, `religion.*`, `text.*` or `depicts.person` tag applies. Any licence field marked `unclear` imports with `ship_blocked: true`, which the M10 release gate refuses.

**Honest uncertainty.** Whether purely AI-generated output can be owned is unsettled. The US Copyright Office's 2025 guidance denies protection to material lacking sufficient human authorship, Indian law has no settled position, and lawsuits over training data are ongoing. Tool terms change, which is why each record snapshots them. The consequences:

- the project may be unable to stop others copying AI-only assets;
- a tool's terms may later change, or turn out to be unenforceable.

Mitigations: prefer tools whose paid terms grant commercial use (ideally with indemnity); record meaningful human edits; keep every AI asset identifiable in coverage so it can be replaced; disclose AI content where stores require it (Steam has since January 2024). This is not legal advice, and legal advice should be taken before release.

## 9. Coverage and the M9 art validation

**The coverage report.** `-run=PWAssetCoverage` writes `Saved/Reports/asset_coverage.json` and `.md`. It breaks the registry down by category and by the milestone that introduced each ID, counting:

- IDs per resolver tier (real, baked, runtime, error);
- origin (owner, third party, AI);
- provenance complete, cultural review pending, ship-blocked;
- contract versions and over-budget assets.

The M8 entry snapshot, for example, would look like this:

| Category | IDs | Real | Baked block | Runtime only | Error | AI origin | Contract tests passing |
|---|---|---|---|---|---|---|---|
| prop | 412 | 0 | 409 | 3 | 0 | 0 | 412 |
| char | 138 | 0 | 138 | 0 | 0 | 0 | 138 |

**Keeping M9 testable from M1.**

- **Scenarios and baselines.** Every scripted scenario (contact, tone, identity, benchmark route, interior suite) and every baseline (gameplay hashes, golden saves, captures) lives in `Tests/Scenarios/` and is rerun at each milestone gate.
- **Labelled findings.** Every playtest finding is labelled by what it establishes: `rules` (comprehension), `pacing`, `block_readability` or `final_presentation`. Only `rules` and `pacing` findings are settled in blocks; `block_readability`, comfort and emotional findings are retested with final art.

**M9 is a second validation, not a bulk import.**

1. **Freeze.** After the M8 gate, tag `art-baseline`. Lock `dimensions_cm`, `envelope_cm`, `gameplay` and every contract with a committed hash file. Changing any of them needs an owner-approved unlock.
2. **Baseline.** Record:
   - per-ID gameplay hashes;
   - 12 golden saves across life phases, each replaying 3 game days of scripted commands, with state hashed every game hour;
   - a collision fingerprint: capsule sweeps of 34 cm radius on a 25 cm grid over every layout;
   - navmesh hashes and contact-test measurements;
   - captures of every tone, identity and appearance-mapping scenario;
   - benchmark numbers.
3. **Import in waves** through route A in batch mode, in this order: characters and animations, architecture, props and items, vehicles and flora, VFX, UI, audio.
4. **After each wave, three separate tests:**
   - **(a) Coverage and provenance.** The wave's IDs are `real`, validated, with complete provenance and no cultural review outstanding.
   - **(b) No unintended change.**
     - Gameplay hashes are byte-identical.
     - Replays reach identical state hashes.
     - Saves are identical apart from build and time fields.
     - The collision fingerprint and navmesh are identical.
     - The contract and rendered-vs-collision tests pass.
     - Physics-driven movement matches within ±1 cm; Chaos is not guaranteed to be bit-deterministic.
   - **(c) Final-presentation acceptance.** Readability (semantic cues, identity at 12 m, expressions); contact tolerances; comfort (first-person clipping, camera clearance); performance against the full budgets; accessibility; and emotional tone, with the `tone.*` scenes re-reviewed by the owner and a cultural reviewer.
5. **Presentation-adjustment period.** At least a third of M9 is reserved for fixing (c) findings in art, animation, lighting and camera. A fix that needs a gameplay-data change is a separate, owner-approved change with its own tests, never part of an art wave.
6. **Reverting.** A failing asset reverts individually with `art.enabled = false`.
7. **Done** when (a) is 100% for shipping IDs (or an ID is explicitly approved by the owner to stay a block), (b) passes, and (c) is signed off. Baked blocks then leave the cook; the runtime builder and error cube stay.
