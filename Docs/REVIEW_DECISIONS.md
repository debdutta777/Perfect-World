> **Archived record, 2026-09-23.** This records decisions taken on the early Sonnet design review, when the project was a mobile-first, isometric, single-district game. Decisions that still hold have been carried into the current documents. The current decision log is `GAME_PLAN.md` §10, and anything here that conflicts with it is superseded.

# Review decisions

Claude Sonnet produced SONNET_DESIGN_REVIEW.md and the first production-plan draft. Review suggestions are proposals; this document records which govern the starter.

- Accepted: make school/home choices visible before supernatural powers; keep an early real Android packaging/performance gate; avoid national-scale content until one small loop works.
- Accepted: family resources provide a bounded reserve and access, not a permanent universal power multiplier. Money, care and stable housing remain separate traits.
- Rejected: automatically attaching high parental expectations or a social penalty to family wealth/support. These would stereotype families and create hidden coupling in a creator intended to explain its effects.
- Deferred: a40% category-spend cap. The current shipped rules remain exactly1,000 points, cumulative costs0/25/60/110/180 and at most four rank4 traits. Add another cap only if small build playtests reveal a concrete balance problem.
- Scoped: life-effect numeric bonuses and XP packages are design proposals, not implemented behavior. The core currently validates ranks and origin records only.
- Scoped: the existing menu is a native desktop development surface. It does not prove mobile usability, full controller navigation or the appearance editor.
- Model policy: latest user instruction makes GPT-5.6 Sol the default for small routine changes. Sonnet complements it; Opus is selective. Do not silently adopt a reviewer's preferred model over the user's preference.

Opus source review was not executed: automatic approval review rejected the external source transfer without specific authorization. Local verification and GPT-5.6 Sol review continued. No source-code payload was sent through that rejected call.
