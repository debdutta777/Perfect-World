# Front-end phase 2 — 12 October 2026

Implementation in source, uncommitted. This record covers the continuation of an interrupted run, not playable acceptance.

- **Intent:** Finish the existing phase-2 adapter and Traits, Household and Stream screens against `UI_FRONTEND_SPEC.md` §§1–4, retaining phase 1 and portable-core authority.
- **Files changed:** Listed below; source changes are confined to `Source/PerfectWorld`. This report is also included; the integrator dropped the audit script and the STATUS.md split.
- **Exact check:** a static audit script run by the implementer (not kept in the repository). Read local UE 5.8 headers/implementations for the APIs listed below. Attempted `git status --short`, `git diff --stat`, and `git diff`; Git was absent from PATH resolution and its installed executable returned Access denied. The audit compares working-tree blobs with the readable version-2 Git index instead. It does not inspect staged differences or packed objects.
- **Result:** PASS for the static audit: scope, UTF-8, files below 500 lines, implementation-owner header first, balanced delimiters, and enum mirror order. Unreal build, PIE and packaged checks: NOT RUN, as instructed. CTest: NOT RUN; portable rules are unchanged.
- **Remaining limitation:** Source inspection cannot prove UHT, compilation, focus routing, layout, text shaping, IME or controller behavior at runtime. See the deferred contracts below.
- **Next smallest action:** Run one UE 5.8 editor build, then the phase-2 interaction checks at 1280×720 and 1920×1080, with keyboard, controller and enlarged text.

## Interrupted work retained

The earlier run had added trait USTRUCT views and subsystem methods, core-backed trait text, a discrete rank selector, category navigator, rank inspector, preset/Clear/Reset drawer, Undo/Redo controls, a sticky budget strip, modal plumbing, and initial responsive Household and Stream pages. It also replaced the care warning in the summary with neutral information. These implementations were reviewed and extended, not recreated.

## Continuation completed

- Consolidated rank-effect formatting for the adapter and legacy review text. Effects and meanings come from `CreationPresentation`; rupees, percentages, XP and caveats now use one formatter. Rank-zero Resources retains the core's authored effect record.
- Added exact serialized core-draft snapshots to both previews and compare them again before applying. A changed preview is refreshed for another deliberate application. Error views include stable codes and arguments; no English-message matching is used.
- Finished rank-row focus inspection, explicit single-stop Tab traversal, independent focus/selected/proposed markers, and controller rank/row input. Fixed delayed focus repeatedly retrying after a composite redirected focus to a descendant.
- Finished modal traversal, safe Cancel focus, preview scrolling, controller navigation, focus restoration from More, and drawer animation. Defaults installed by BeginNewCreation do not become a player Undo step.
- Finished Household's compact 35/65 layout, adapter-provided everyday copy, remembered-household presentation in care, inline selection errors, Resources link, and detail transition.
- Finished Stream's equal-height choices, 60/40 details at wide size, stacked narrow layout, retained optional reason, core validation on commit, inline errors, and detail transition. Invalid reason text remains editable rather than being truncated by an arbitrary Slate limit.
- Added F6 traversal and keyboard/controller scrolling through the single creator body. The logical viewport breakpoint is supplied by the root, so all three pages reflow together. Creator bounds, padding and navigation/footer/budget minimum heights follow the two target layouts. Narrow Traits keeps Apply rank and its charge/refund beside the sticky budget even while the inspector scrolls. Step accessibility reports Current or the core's whole-draft validation verdict; per-field step validation awaits core metadata.

## File manifest

| Files | Purpose |
|---|---|
| `Public/PerfectWorldCharacterCreatorSubsystem.h`, `Public/PerfectWorldCreationViews.h` | Additive reflected presentation, preview, history and future stream contracts; enum order retained. |
| `Private/PerfectWorldCreationAdapter.h/.cpp` (new) | Core presentation formatting, groups, rank/allocation previews, authored household copy, and history bridge. |
| `Private/PerfectWorldCharacterCreatorFlow.cpp` | Neutral care summary, stream TODOs, and fresh-draft history boundary. |
| `Private/PerfectWorldCharacterCreatorSubsystem.cpp` | Populate household everyday presentation. |
| `Private/UI/PWTraitText.h/.cpp` | Replace hard-coded effects with core presentation, sharing adapter formatting. |
| `Private/UI/SPWRankSelector.h/.cpp` (new) | Accessible discrete rank proposals and keyboard/controller behavior. |
| `Private/UI/SPWTraitsPage.h/.cpp` | Toolbar, navigator, persistent rows, proposal lifecycle and focus regions. |
| `Private/UI/SPWTraitsInspector.cpp`, `SPWTraitsDrawer.cpp` (new) | Rank comparison/ladder, sticky budget, and preview-before-apply operations. Split implementations include their owning class header first. |
| `Private/UI/SPWHouseholdPage.h/.cpp` | Household and resolved-home details with neutral care presentation. |
| `Private/UI/SPWStreamPage.h/.cpp` | Stream choices, description fallback, optional reason and responsive details. |
| `Private/UI/SPWCreationFlow.h/.cpp`, `SPWCreationFlowInput.cpp` (new) | Sticky budget integration, modal/focus/scroll routing and shared viewport reflow. Split input includes its owning class header first. |
| `Private/UI/SPWFrontEndRoot.cpp` | Exact creator bounds and logical narrow-layout attribute. |
| `Private/UI/PWDelegates.h`, `PWWidgets.cpp` | Detail-focus delegate, persistent selection/accessibility, wrapped labels, and composite-focus completion. |

Source paths in the table are relative to `Source/PerfectWorld`.

## UE 5.8 APIs inspected because their exact contracts were uncertain

- `SWidget::SetAccessibleBehavior(EAccessibleBehavior, const TAttribute<FText>&, EAccessibleType)` and `GetAccessibleText(EAccessibleType)`; accessibility-only calls are guarded by `WITH_ACCESSIBILITY`.
- `FReply::SetUserFocus(TSharedRef<SWidget>, EFocusCause)` and `FNavigationReply::Explicit(TSharedPtr<SWidget>)` / `Stop()`; these support composite rank focus and region traversal.
- `SScrollBox::SetIsFocusable(bool)`, `ScrollDescendantIntoView(TWeakPtr<SWidget>, bool, EDescendantScrollDestination)`, `GetScrollOffset()` and `SetScrollOffset(float)`; scrollable detail focus uses the existing body.
- `SWidget::GetType()` / `GetTypeAsString()` for modal scroll identification and preserving text-editing keys.
- `TSlateDecl::Expose(TSharedPtr<ExposeAsWidgetType>&)` permits `SAssignNew` into a base-widget pointer.
- `SEditableTextBox::MaximumLength` rejects input at the widget boundary; it was removed in favor of core validation on commit. `SButton` defaults to `EButtonPressMethod::DownAndUp`, so modal click callbacks run on activation release; rank selectors consume activation themselves.
- `FCurveSequence::AddCurve` / `Play`, `FCurveHandle::GetLerp` and `SWidget::SetRenderOpacity` for 160 ms detail reveals and the 180 ms drawer reveal.

These were inspected in the locally installed engine source. No engine compilation was performed.

## Deferred or constrained by missing core fields

- Stream sample-day rows, route-flexibility/equal-starting-XP text, and the new exact card summaries are absent from the immutable core. The existing authored timetable descriptions remain visible; no schedule, route rule or new XP promise was invented. Clearly named adapter TODOs identify the missing fields.
- Exact trait descriptions, per-rank meanings, group punctuation and some household wording in the spec differ from the core's current English records. The UI displays those core strings as requested. Creative's core caveat remains visible; it is not presented as an active skill.
- The core supplies no draft revision, ranked highlight list or grapheme validation metadata. Previews use exact state snapshots; the adapter sorts existing ranks for three display highlights with catalogue-order ties. Household everyday prose is authored spec copy in the adapter pending a portable presentation field.
- No voxel home vignette is available to this UI, so its space is omitted.
- Reduced motion and the remaining phase-3 screens/contracts are deferred as instructed.
- Runtime acceptance at both resolutions, 150% text, translated strings, Bengali IME, repeated history/save/load and failure paths remains unverified.
