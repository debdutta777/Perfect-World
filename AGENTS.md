# Perfect World agent workflow

**Updated 2026-09-23.** Start with `Docs/GAME_PLAN.md`, the master plan. `Docs/DEVELOPMENT_WORKFLOW.md` is the full working procedure; this file is its short form for any agent, Claude or Codex, arriving cold.

## Model allocation

Owner preference: use GPT-5.6 Sol for routine C++, tests, data authoring and implementation when working through Codex. In a Claude Code session, Claude implements and integrates in the repository. **Send design critique and second opinions to GPT-6 (`gpt-6-astra` via Codex)**; the owner asked for this on 2026-09-20 and again on 2026-09-23. Claude Sonnet also suits narrative, dialogue and second reviews. Reserve Claude Opus for difficult architecture, concurrency, migration, security or stubborn debugging. For easy read-only checks, naming consistency or simple mechanical edits, use a lower-cost model such as GPT-5.6 Luna when available, or deterministic tools with no model call at all.

Split bounded tasks with explicit file ownership. Never claim a requested model ran unless execution evidence exists. Do not silently replace an unavailable model with a more expensive one. `codex exec` takes its prompt on stdin. The model id `gpt-6` is refused on this ChatGPT account, and `gpt-6-astra` works. When a model hits its usage limit, preserve the partial output and report it; do not retry in a loop, and do not assume a different model bypasses account-wide limits.

This file is project guidance, not a global app model setting. Do not enable permissions bypass or recursive autonomous delegation. Never include secrets in artifacts or logs.

## Product truth

Current scope, decided 2026-09-20 and 2026-09-23 (the full decision log is in `GAME_PLAN.md` §10):

- **A fully alternate near-future world at world level**: thirty nations, national and world competitions, public rankings, Country Level. The player starts at exactly 16 in Class XI in Kolkata, West Bengal, and may live an entire life.
- **Full life freedom. The supernatural is optional.** A player who never fights has a complete game. No path is the lesser path.
- **Unified Awakening** ten days before a virtual-tower national examination, through a school tutorial dungeon.
- **1,000 creation points.** Identity and appearance are free. No paid point purchases and no bought supernatural class. Household Resources rank 0 means growing up in institutional care.
- **First-person camera by default, switchable to third person.**
- **Voxel block visuals for the whole of development.** Real art is integrated only after the game is complete (milestone M9). Gameplay never depends on meshes; everything visual is referenced by asset ID (`Docs/ASSET_PIPELINE.md`).

## Implementation

Read `Docs/GAME_PLAN.md`, `Docs/SYSTEMS.md`, `Docs/ROADMAP.md` and `Docs/BUILD_GUIDE.md` before implementation, plus the design document for the area being touched. Portable rules are authoritative; do not mirror math in UI. Keep IDs stable. Use C++ for rules and data for content. Test rules with CMake/CTest and verify Unreal separately. Preserve engine version unless migration is requested and tested. Track planned, implemented and verified status independently, and never claim a higher verification rung than was actually run.

**Environment traps, detailed in `DEVELOPMENT_WORKFLOW.md` §4:** bash heredocs collapse backslash escapes, so write C++ and JSON with file-editing tools. Build the portable core with a short build directory. The Unreal editor must be closed to rebuild. Never run two Unreal builds at once.

## Integration

When working on a complex multi-file change, discover applicable Ruflo MCP tools and use routing or shared memory if available. MCP registration alone is not agent execution. Keep provenance for generated art and prompts. Concept art is reference material, not evidence of playable assets.

**Commits carry no `Co-Authored-By` trailer.** The owner's global `CLAUDE.md` forbids it unless `.claude/settings.json` sets `attribution.commit`, and this project does not. Push only when the owner asks or has authorised it for the session.

## Small-step execution

Choose one bounded step from `Docs/ROADMAP.md` (usually one to three files), state its completion criterion, implement, run the relevant check, and record the result in `Docs/STATUS.md`. Keep reasoning and context small. Do not launch open-ended autonomous builds. Escalate difficulty based on an actual unresolved issue, not file count alone.
