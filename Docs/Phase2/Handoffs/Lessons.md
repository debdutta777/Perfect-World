# Lessons handoff: one shared lesson shell (A2 + A3 consolidated)

Branch `ui/p2-lessons` (worktree `D:\PWWork\worktrees\w-lessons`), written by Claude (Sonnet 5.5). **Nothing is committed. Nothing was compiled in Unreal; no Unreal build or automation test was run** (see "Not run"). This replaces the shell halves of `A2.md` and `A3.md`; read those for each lesson's own design (prices, beats, hidden fields), which is unchanged.

## What changed

| File | What |
|---|---|
| `Source/PerfectWorld/Private/UI/SPWClassLesson.h` | The ONE shell `SPWClassLesson`, the namespace `PWClassLesson` (layout, words, `IBridge`, `FCoreBridge`, `FFrame`, `FOutcomeFacts`, `FShellArgs`) and `FPWClassLessonClosed`. |
| `UI/SPWClassLesson.cpp` | Shell: construction, refresh, focus restore, keys, Back, mode/goal selection (510 lines). |
| `UI/SPWClassLessonCommands.cpp` | Shell: open, close/leave early, choose, the action registry (229). |
| `UI/SPWClassLessonBody.cpp` | Shell: rows, idle / finished / summary panes, split-or-stacked layout (541). |
| `UI/SPWClassLessonWords.cpp` | Words, `FCoreBridge`, `BuildFrame` (228). |
| `UI/SPWClassLessonTable.cpp` | The English table, **generated** from the two JSON bundles (351 keys). Edit the JSON and this table together; a test checks they agree. |
| `UI/SPWClassModel`, `SPWClassExplain`, `SPWClassInvestigate`, `SPWClassCoordinate` (.h/.cpp) | Now only the four structure bodies, all `: public SPWClassLesson`. The old `SPWClassLessonBase`, `SPWClassLessonShell`, `PWClassA3`, `ILessonBridge`, `FCoreLessonBridge` are gone. |
| `PerfectWorldPhase2A2Tests.cpp` | Model + Explain tests and the shared shell/words tests (`PerfectWorld.Phase2A2.*`). |
| `PerfectWorldPhase2A3Tests.cpp` | Investigate + Coordinate tests (`PerfectWorld.Phase2.A3.*`; one new: Back leaves open, a new screen resumes, Leave early asks twice). |
| `Data/Text/en/phase2_a2.json` (203 keys), `phase2_a3.json` (148 keys) | Merged; no key is in both. |

## Which idea was kept from which order, and why

From A3 (the shell mechanics):
* **The action registry**: every pressable thing is a real `SButton` row registered under a string key (`PressAction`, `HasAction`, `IsActionEnabled`). It makes the screens testable and gives focus restoration an index to work from.
* **Focus restored after every rebuild** to the nearest enabled action at or after the old position, else before it, else Back. (A2 only refocused the first control, losing the player's place.) `OnFocusChanging` scrolls the focused row into view without animation.
* **Back and Leave early outside the scroll area**; the banner is outside it too; compact density hides eyebrow, subtitle and key hints.
* **Idle = choose mode (Foreground / Routine / Attend), goal, then Start**, with G / M and shoulder-button cycling, rather than A2's three cards with one button each: all reasons show in words in one place and it works with the controller.
* Period line with bell times and room, `OnLessonClosed(bool bCancelled)`, `ResolveText` on the bridge (A1's title/brief/role keys come from the host's table), short prices ("Free", "2 units", "Needs 4, 2 left").

From A2 (safety and structure):
* **Two-press Leave early** (A3's was one press for an irreversible, slot-burning action). First press arms and explains, the second sends; any other action disarms. `ExecuteClose(true)` itself sends at once (tests, hosts).
* **Elapsed minutes from the core constant `PerfectWorld::ModelUnitMinutes`** (A3 hard-coded 5 and said the core did not expose it; it does).
* **Two-pane layout** (case context left, choices right) at >= 1160x660 logical, stacked in one scroll below, switched on `Tick` (`RefreshLayout`). Split is used only while an exercise runs.
* **Summary pane after a close** (time recorded; learning credited = sum of the Completed events' `MilliXp`), **resume notice**, **wrong-screen guard** (a screen never drives the other kind of lesson), **`FOutcomeFacts`** (one outcome description for all structures; handles `ModelOutcome`'s separate grade type), `IssueText` over `school.session.*`, `model.*`, `structure.*`, `dayloop.*` with a neutral fallback so a key is never shown, and the **code English table with a drift test**.

Thresholds: density `Compact` uses A3's switch (X or Y < 760 logical, the quest window's); pane arrangement uses A2's (1160x660). At 1280x720 that is compact density with two panes.

## The Back / resume conflict, resolved as ONE behaviour

A3 said Back leaves the lesson open and the desk must reopen it; A2 said a new screen on the same world resumes the session. Both are true, and now they are one rule:

> **Back, Esc, B and the menu button only leave the screen** (they call `OnClose`, first press only, then give keyboard focus to `ReturnFocus`). They never close or cancel the lesson: the session is core state (`SchoolSessions.Active`). **Constructing a new screen of the right class over a bridge for the same binding on the same world resumes the lesson at its current step**, with a "you are back in the lesson" banner. Selections and the Coordinate draft are widget-local and start empty; everything already chosen is in the core's exposure and is shown. **The host must therefore route the classroom desk to reopen the screen for `QuerySchoolLesson(...).Run->Binding` whenever a run exists** (and keep free-roam time suspended while `Active` exists). A run for another binding shows "another lesson is in progress" with Back only. **Leave early is the only exit that ends the period** (two presses; charges working time, earns no learning, still counts as a foreground slot); Record is the other.

`OnLessonClosed` fires once, after the core accepted Record or Leave early. **The host should NOT drop the panel there**: the panel then shows the summary, and Back is the exit. (A3's handoff said the host normally closes there; change that.)

## The one bridge interface (full)

```cpp
namespace PWClassLesson
{
class IBridge
{
public:
    virtual ~IBridge() = default;
    virtual const PerfectWorld::SchoolLessonBinding& GetBinding() const = 0;
    virtual const PerfectWorld::SchoolSessionCatalogues& GetCatalogues() const = 0;
    virtual PerfectWorld::SchoolPeriodView QueryPeriod() const = 0;                  // QuerySchoolPeriod
    virtual PerfectWorld::SchoolLessonView QueryLesson() const = 0;                  // QuerySchoolLesson
    virtual PerfectWorld::SchoolSessionResult OpenLesson(PerfectWorld::SchoolLessonMode Mode,
        const PerfectWorld::SchoolLessonGoal& Goal) = 0;                              // school.lesson.open
    virtual PerfectWorld::SchoolLessonQuote QuoteAction(uint64 Session,
        const PerfectWorld::SchoolLessonAction& Action) const = 0;                    // QuoteSchoolLessonAction, a pure read
    virtual PerfectWorld::SchoolSessionResult ChooseAction(uint64 Session,
        const PerfectWorld::SchoolLessonAction& Action) = 0;                          // school.lesson.choose
    virtual PerfectWorld::SchoolSessionResult CloseLesson(uint64 Session, bool bCancel, int32 ElapsedMinutes) = 0; // school.lesson.close
    /** A1's TitleTextKey / BriefTextKey / RoleTextKey. Empty = unknown (title falls back to the family name; brief and role are hidden). */
    virtual FText ResolveText(const FString& Key) const;   // default: PWClassLesson::FindText, else empty
};

// Direct calls over borrowed state, for tests and a harness.
class FCoreBridge final : public IBridge
{
public:
    FCoreBridge(PerfectWorld::SchoolSessionsState&, PerfectWorld::DayLoopState&, const PerfectWorld::SchoolSessionCatalogues&,
                PerfectWorld::SchoolLessonBinding, int32 SkillMilli, uint64 WorldSeed,
                std::vector<PerfectWorld::ReceiptCandidate> KnownAlternatives = {});
    /* overrides of the seven calls above */
};
}
```

Skill level, world seed and known alternatives for open come from the R9 authority and current commitments **inside the bridge / reducer**, never from a screen. Map a rejected command's code to `SchoolIssue{Code}` unchanged (the screens turn the code into words). Feed `SchoolSessionResult::Events` to `ApplyActivityCredits` inside the same transaction, as the P2C notes say; the screens only read `Ok()`, `Issue`, `ReadyToClose`, `ChargedUnits`, and the Completed events' minutes and `Credited` for the summary.

The production bridge, shape (a sketch; the command wrappers are the P2C notes'; not compiled):

```cpp
class FWorldLessonBridge final : public PWClassLesson::IBridge
{
    // GetBinding / GetCatalogues: the A1 binding for this desk and the immutable SchoolSessionCatalogues.
    // QueryPeriod  -> QuerySchoolPeriod(State.SchoolSessions, State.DayLoop, Binding, Cats)
    // QueryLesson  -> QuerySchoolLesson(State.SchoolSessions)
    // QuoteAction  -> QuoteSchoolLessonAction(State.SchoolSessions, Cats, Session, Action)     (no command)
    // OpenLesson   -> Submit(SchoolLessonOpen{Binding.Id, Mode, Goal})   (skill, seed, alternatives from R9 here)
    // ChooseAction -> Submit(SchoolLessonChoose{Session, Action})
    // CloseLesson  -> Submit(SchoolLessonClose{Session, bCancel, ElapsedMinutes})
    // ResolveText  -> A1's text table for the binding's keys; fall back to the base class for phase2.* keys.
};
```

After any change made outside a screen, call `Screen->RefreshModel()`.

## Integrator snippets (literal)

Includes (wherever the lesson screens are created, e.g. A1's `SPWSchoolDay` factory):

```cpp
#include "UI/SPWClassModel.h"
#include "UI/SPWClassExplain.h"
#include "UI/SPWClassInvestigate.h"
#include "UI/SPWClassCoordinate.h"
```

Four dispatch entries (anchor: the switch over `Binding.Structure` in the factory). All four take the same five arguments:

```cpp
switch (Binding.Structure)
{
case PerfectWorld::SchoolStructure::Model:
    Screen = SNew(SPWClassModel)
        .Bridge(LessonBridge)                                  // TSharedPtr<PWClassLesson::IBridge>
        .Space_Lambda([Self]() { return SPWPauseMenu::PauseSpaceOf(Self); })   // logical size, as the pause menu passes to its windows
        .OnClose(FSimpleDelegate::CreateSP(this, &SPWSchoolDay::LeaveLessonScreen))   // Back: leave the screen; the lesson stays open
        .OnLessonClosed(FPWClassLessonClosed::CreateSP(this, &SPWSchoolDay::LessonPeriodClosed)) // Record / Leave early accepted; do NOT drop the panel here
        .ReturnFocus(OpenerWidget);                            // TWeakPtr<SWidget>: the desk prompt / button that opened it
    break;
case PerfectWorld::SchoolStructure::Explain:
    Screen = SNew(SPWClassExplain)     /* the same five arguments */;
    break;
case PerfectWorld::SchoolStructure::Investigate:
    Screen = SNew(SPWClassInvestigate) /* the same five arguments */;
    break;
case PerfectWorld::SchoolStructure::Coordinate:
    Screen = SNew(SPWClassCoordinate)  /* the same five arguments */;
    break;
}
```

Renames against the old handoffs: A2's `ReturnFocusTarget` and `OnClosed` are now `ReturnFocus` and `OnClose`; A2's `RefreshFromSession()` is `RefreshModel()`; A2's `Trigger*` are `Execute*`; A3's `PWClassA3::ILessonBridge` is `PWClassLesson::IBridge`; A2's `TriggerOpenLesson(Mode)` is `ExecuteOpen(Mode)`; A2's `SelectGoal(enum)` is `SetGoalIndex(i)`.

Desk interaction (resume): while `QuerySchoolLesson(State.SchoolSessions).Run` is set, interacting with a classroom desk must open the screen for `Run->Binding`. If that is not the desk's own binding the screen says "another lesson is in progress" and offers only Back.

Focus: `ReturnFocus` is held weakly. `RequestClose()` cancels pending focus work, runs `OnClose` (the host removes the screen), then sets keyboard focus on the target if it is alive. The pause-menu pattern also works: omit `ReturnFocus` and call `PWUI::FocusWhenReady(AsShared(), RestoreTarget)` in the close handler. While open, the shell restores focus to the nearest enabled action after every rebuild.

No CMake, Build.cs, Config or content-manifest change: the module globs `Private/**.cpp`, and `Data/Text` is outside `Data/Content`. If any explicit source list exists, add the five new `.cpp` files (`SPWClassLesson`, `SPWClassLessonCommands`, `SPWClassLessonBody`, `SPWClassLessonWords`, `SPWClassLessonTable`).

## Text tables

One code table (`SPWClassLessonTable.cpp`) equals `phase2_a2.json` + `phase2_a3.json`, checked by `PerfectWorld.Phase2A2.TextTableMatchesTheJsonBundles` (which also checks no key is in both). Shared shell strings live under `phase2.a2.common.*` and `phase2.a3.common.*`; structure strings keep their `phase2.a2.model|explain.*` and `phase2.a3.investigate|coordinate.*` keys; refusals are `school.session.*` plus `phase2.a2.issue.*`.

Removed as redundant (the shell uses the other order's equivalent):
* a2: `common.cancel_lesson` (-> a3 `common.leave`); `cancel_note.foreground`, `cancel_note.other` (-> a3 `leave_note`); `leave_panel` (-> a3 `back`); `mode.attend|foreground|routine` and their `.desc`, and `open_attend|foreground|routine` (-> a3 `mode.*`, `mode.*.hint`, Start); `mode_heading` (-> a3); `no_bridge` (-> a3 `unavailable`); `other_session` (-> a3 `other_lesson`); `period_header` (-> a3 `period_line`); `quote.cost|free|unavailable` (-> a3 `cost.*` and the issue sentence); `units_remaining` (-> a3 `units`).
* a3: `common.record`, `record_hint` (-> a2 `complete_lesson`, `outcome.ready`); `goal_heading` (-> a2); `mode.unavailable.foreground|routine` (-> the core's own `school.session.foreground_limit|not_secure`); `grade.*`; `outcome_heading`; `outcome.grade|units|secure|no_faults|routine|attend`; `opened`; `recorded`; `left` (-> a2 equivalents); and all 28 `issue.*` (-> a2 refusal sentences).
* Reworded, same key: a2 `common.cancel_note`, `confirm_cancel` ("Confirm: leave early"), `status.cancelled`, `summary.cancelled`; a3 `common.leave_note`.
* Added: a2 `common.resumed`; a3 `investigate.eyebrow|title`, `coordinate.eyebrow|title`, `coordinate.assign.price`.

## Guarantees and how each is met

* **Every price is a core quote**: rows call `IBridge::QuoteAction` with the displayed session while building; the price shown is `PriceText(quote)`. Charges shown are the core's `ChargedUnits`.
* **Refusals in words from the core's code**: `IssueText(code)`; unknown codes read neutrally; a row whose quote is refused is disabled and shows the sentence; period and mode reasons come from the core's flags and codes.
* **Hidden solution fields are never read** (list in the header comment of `SPWClassLesson.h`). The only fault-derived reads: `InvestigateTestPositive` for tests already run (what running reveals) and `ModelExplanationOwed` (a public rule that only pre-ticks a toggle).
* **Every command carries the displayed session id** (`DisplayedSession`, refreshed with each `RefreshModel`); a screen the world moved on from is refused by the core.
* **Back and Leave early stay on screen at 853x480 and 150% text** (569x320 logical): both are in the footer outside the scroll box; header, banner and hints shrink or hide when compact; the body scrolls.
* **Controller focus is restored after a rebuild**; Up/Down/D-pad/stick by Slate navigation; Enter/A; G/RB and M/LB (idle only); Esc/B/menu (first press only); PageUp/PageDown and right stick scroll.
* **No motion** anywhere (scroll-into-view is not animated).

## Assumptions and risks

1. The production bridge, the reducer command wrappers and the `SchoolSessions` member of `WorldState` do not exist in this base; everything is exercised only through `FCoreBridge`.
2. Binding title/brief/role keys are A1's; until the host's `ResolveText` knows them the title is the family name and brief/role are hidden.
3. Goal rows for Model and Explain have no hint text; Investigate and Coordinate keep A3's hints.
4. Leave early stays armed until another action, any result, or a new session; it does not time out.
5. Coordinate: the commit row is disabled until every task has a pencilled member (the draft is never sent incomplete); the price of committing is shown beforehand.
6. A finished Foreground run cannot be left early (the core refuses; the button is hidden). Routine and Attend runs can be left early at 0 minutes.
7. `Space` is the project's logical size attribute; default 1920x1080.

## First-compile risks

* `SLATE_ARGUMENT(TWeakPtr<SWidget>, ReturnFocus)` given a `TSharedPtr`; `SLATE_ARGUMENT(TSharedPtr<IBridge>, Bridge)` given a `TSharedRef<FCoreBridge>` (both implicit conversions).
* `TAttribute<FText>::CreateSPLambda(this, ...)` and `FOnClicked::CreateSPLambda(this, ...)` need the screen to be shared-owned (true after `SNew`; `ConstructShell` runs from `Construct`).
* `FText::Format(PWClassLesson::Text(...), A, B)` relies on the implicit `FText` to `FTextFormat` conversion (A2 and A3 both did).
* `SAssignNew(...)` followed by `+ SScrollBox::Slot()` inside a function argument (`SetLayout`); the persistent `SVerticalBox`es are re-parented after `SetContent(SNullWidget::NullWidget)` releases the old tree.
* `PerfectWorld::ModelNameOf(...)` and the `...Names[static_cast<size_t>(x)]` tables (copied from A2/A3, never compiled together).
* `PWClassLesson::IssueText` is overloaded for `std::string` and `SchoolIssue`; a `const char*` converts to `std::string`.
* Tests: `Panel->OnKeyDown(FGeometry(), FKeyEvent)` and button lookup by text, as in A2/A3.
* Unity build: file-local names carry `Cl` (`ClW`, `ClB`, `ClCmd`, `ClS`, `ClM`, `ClE`, `ClI`, `ClC`, `ClT`, test namespace `ClT3`); the A2 test file keeps its `A2T` prefix and the A3 test file `A3Tst`; no file-scope `using namespace` (A3 tests use function-local directives and a nested `ClT3` namespace). `FPWClassLessonClosed` is a global delegate type and must stay unique in the module.

## Not run

Every Unreal compile, link and unity build; every automation test (`PerfectWorld.Phase2A2.*`, `PerfectWorld.Phase2.A3.*`); PIE; rendering at 853x480 / 569x320 / 1280x720 / 1920x1080 and 150% text; real focus and controller travel; the reduced-motion check; the world-backed bridge and host integration; any human or accessibility review. No portable file changed, so A2/A3's Rung 1 result (206/206) is untouched but was not re-run here. The two JSON bundles and the C++ table were produced by a script; the literal keys used in code were checked against the merged key set, and dynamic key families (goal, role, scarce, question, symptom names) were not re-verified beyond reading.
