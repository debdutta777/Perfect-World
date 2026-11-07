#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "SchoolSessions.h"

class SBox;
class SScrollBox;
class SVerticalBox;
class SWidget;

/** Called once after the lesson period was closed through the core (Record or Leave early); true when it was left early. */
DECLARE_DELEGATE_OneParam(FPWClassLessonClosed, bool /*bCancelled*/);

/**
 * The one shared classroom lesson shell (consolidates orders A2 and A3). Four screens sit on it: SPWClassModel, SPWClassExplain,
 * SPWClassInvestigate and SPWClassCoordinate. Files: SPWClassLesson.cpp (shell), SPWClassLessonBody.cpp (rows and panes),
 * SPWClassLessonWords.cpp (words, bridge, frame), SPWClassLessonTable.cpp (the English table).
 *
 * Authority. The screens hold no rules. Every price on screen is a core quote (QuoteSchoolLessonAction) and every charge is the core's own
 * ChargedUnits; every grade and diagnostic is the core's typed outcome; every refusal is shown in words chosen from the core's code. The only
 * things sent are school.lesson.open / .choose / .close through IBridge, and every choose and close carries the session id the screen last
 * displayed, so a stale screen is refused by the core, never by a widget. A screen never reads a hidden solution field (ModelLink::Sound,
 * ModelCaseDef::Fault, ExplainClaim::Defensible, ExplainEvidence::Supports/Against, ExplainLimitation::Bounds, InvestigateCause::Fault,
 * CoordinateFact::Critical, CoordinateCaseDef::Constraint/Seam) for display and never calls a *StandardAction policy. The one fault-derived
 * value is the reading of an investigate test the player has already run, which is what running it reveals.
 *
 * One behaviour for Back. Back, Esc, B and the menu button only LEAVE THE SCREEN (OnClose). They never close or cancel the lesson: the
 * session is core state (SchoolSessions.Active). Constructing a new screen of the right class over a bridge for the same binding on the same
 * world resumes the lesson at its current step; selections and the coordinate draft are widget-local and start empty. The host therefore
 * must route the classroom desk to reopen the screen for QuerySchoolLesson(...).Run->Binding while a run exists. A run for another binding
 * shows a note and Back only. Leave early is the only way out that ends the period, and it asks twice.
 *
 * Keys: Up/Down/D-pad/left stick move between actions (Slate navigation, the body scrolls to the focused one); Enter/A presses; G or right
 * shoulder changes the goal and M or left shoulder the mode (before the lesson starts); Page Up/Down and the right stick scroll; Esc/B/menu
 * go back (first press only). No motion is used anywhere, so reduced motion holds by construction.
 */
namespace PWClassLesson
{
// ---- Layout (logical units: the project's Space attribute, as the pause menu passes to its windows) ----

inline FVector2D SafeSpace(FVector2D Space)
{
	return FVector2D(FMath::Max(Space.X, 320.0), FMath::Max(Space.Y, 240.0));
}

/** The compact density, the same switch as the quest window: 853x480 at 150% text is about 569x320 logical. */
inline bool Compact(FVector2D Space)
{
	const FVector2D S = SafeSpace(Space);
	return S.X < 760.0 || S.Y < 760.0;
}

/** Case context left, choices right: only at 1160x660 logical or more. Below it the same content is stacked in one scroll. */
inline bool TwoPane(FVector2D Space)
{
	const FVector2D S = SafeSpace(Space);
	return S.X >= 1160.0 && S.Y >= 660.0;
}

inline float PanelWidth(FVector2D Space)
{
	const FVector2D S = SafeSpace(Space);
	const float Margin = Compact(Space) ? 24.0f : 96.0f;
	return FMath::Clamp(static_cast<float>(S.X) - Margin, 280.0f, 1180.0f);
}

inline float PanelMaxHeight(FVector2D Space)
{
	const FVector2D S = SafeSpace(Space);
	const float Margin = Compact(Space) ? 20.0f : 76.0f;
	return FMath::Clamp(static_cast<float>(S.Y) - Margin, 200.0f, 880.0f);
}

/** Widget tags the tests search for (widgets carry FName(tag)). */
inline const TCHAR* const PanelTag = TEXT("PWClassLessonPanel");
inline const TCHAR* const RowTag = TEXT("PWClassLessonRow");
inline const TCHAR* const BannerTag = TEXT("PWClassLessonBanner");
inline const TCHAR* const OutcomeTag = TEXT("PWClassLessonOutcome");
inline const TCHAR* const SummaryTag = TEXT("PWClassLessonSummary");
inline const TCHAR* const NoteTag = TEXT("PWClassLessonNote");

// ---- Words (pure; English until the text pipeline loads Data/Text) ----

struct FTextEntry
{
	const TCHAR* Key;
	const TCHAR* Value;
};
/** The English table, generated from Data/Text/en/phase2_a2.json and phase2_a3.json (SPWClassLessonTable.cpp). */
TArrayView<const FTextEntry> TextEntries();

/** English for a text key. False, with OutText untouched, for an unknown key. */
bool FindText(const FString& Key, FText& OutText);
/** English for a known key, else a neutral sentence. A key is never shown. */
FText Text(const TCHAR* Key);
FText Text(const FString& Key);
FText Text1(const TCHAR* Key, const FText& A);
FText Text2(const TCHAR* Key, const FText& A, const FText& B);
FText Text3(const TCHAR* Key, const FText& A, const FText& B, const FText& C);
FText Number(int64 Value);
/** Core and content strings are UTF-8. */
FString ToFString(const std::string& Value);
std::string ToUtf8(const FString& Value);
FText FromUtf8(const std::string& Value);
/** The sentence for a refusal code (school.session.*, model.*, structure.*, dayloop.*); a neutral sentence for an unknown code. */
FText IssueText(const std::string& Code);
FText IssueText(const PerfectWorld::SchoolIssue& Issue);
/** "09:35" from a minute of the day. */
FText ClockText(int32 MinuteOfDay);
/** "Free", "1 unit" or "3 units". */
FText CostText(int32 Units);
/** The price shown beside a choice, read only from the core's quote: CostText, or "Needs N, M left" when the core says time is short,
 *  or empty for any other refusal (the sentence for that is the row's detail). */
FText PriceText(const PerfectWorld::SchoolLessonQuote& Quote, int32 UnitsLeft);
/** "Time left: 4 of 7 units". */
FText UnitsText(int32 Left, int32 Total);
FText ModeText(PerfectWorld::SchoolLessonMode Mode);
FText ModeHint(PerfectWorld::SchoolLessonMode Mode);
FText GradeText(PerfectWorld::StructureGrade Grade);
/** Every key of the English table, for the key-drift test. */
TArray<FString> KnownTextKeys();

// ---- The seam to the world: the ONE bridge interface ----

/**
 * What a lesson screen needs from the world, and nothing else. The integrator's implementation submits school.lesson.open / .choose /
 * .close through the reducer in one atomic transaction each; FCoreBridge below calls the portable contract directly over borrowed state, for
 * tests and a standalone harness. Skill, world seed and known alternatives belong to the bridge, never to a widget.
 */
class IBridge
{
public:
	virtual ~IBridge() = default;

	virtual const PerfectWorld::SchoolLessonBinding& GetBinding() const = 0;
	virtual const PerfectWorld::SchoolSessionCatalogues& GetCatalogues() const = 0;
	virtual PerfectWorld::SchoolPeriodView QueryPeriod() const = 0;
	virtual PerfectWorld::SchoolLessonView QueryLesson() const = 0;
	/** school.lesson.open. */
	virtual PerfectWorld::SchoolSessionResult OpenLesson(PerfectWorld::SchoolLessonMode Mode, const PerfectWorld::SchoolLessonGoal& Goal) = 0;
	/** QuoteSchoolLessonAction: a pure read, no command. */
	virtual PerfectWorld::SchoolLessonQuote QuoteAction(uint64 Session, const PerfectWorld::SchoolLessonAction& Action) const = 0;
	/** school.lesson.choose. */
	virtual PerfectWorld::SchoolSessionResult ChooseAction(uint64 Session, const PerfectWorld::SchoolLessonAction& Action) = 0;
	/** school.lesson.close: ElapsedMinutes is 0 unless bCancel. */
	virtual PerfectWorld::SchoolSessionResult CloseLesson(uint64 Session, bool bCancel, int32 ElapsedMinutes) = 0;
	/** The words for one of the binding's text keys (A1 owns TitleTextKey, BriefTextKey, RoleTextKey). Empty when unknown: the screen then
	 *  falls back to the family name for the title and hides the brief and the role. */
	virtual FText ResolveText(const FString& Key) const
	{
		FText Found;
		return FindText(Key, Found) ? Found : FText::GetEmpty();
	}
};

/** Direct adapter over SchoolSessionsState + DayLoopState + the borrowed catalogues. They must outlive it. */
class FCoreBridge final : public IBridge
{
public:
	FCoreBridge(PerfectWorld::SchoolSessionsState& InSessions, PerfectWorld::DayLoopState& InDay,
		const PerfectWorld::SchoolSessionCatalogues& InCatalogues, PerfectWorld::SchoolLessonBinding InBinding,
		int32 InSkillMilli, uint64 InWorldSeed, std::vector<PerfectWorld::ReceiptCandidate> InKnownAlternatives = {});

	virtual const PerfectWorld::SchoolLessonBinding& GetBinding() const override { return Binding; }
	virtual const PerfectWorld::SchoolSessionCatalogues& GetCatalogues() const override { return Catalogues; }
	virtual PerfectWorld::SchoolPeriodView QueryPeriod() const override;
	virtual PerfectWorld::SchoolLessonView QueryLesson() const override;
	virtual PerfectWorld::SchoolSessionResult OpenLesson(PerfectWorld::SchoolLessonMode Mode, const PerfectWorld::SchoolLessonGoal& Goal) override;
	virtual PerfectWorld::SchoolLessonQuote QuoteAction(uint64 Session, const PerfectWorld::SchoolLessonAction& Action) const override;
	virtual PerfectWorld::SchoolSessionResult ChooseAction(uint64 Session, const PerfectWorld::SchoolLessonAction& Action) override;
	virtual PerfectWorld::SchoolSessionResult CloseLesson(uint64 Session, bool bCancel, int32 ElapsedMinutes) override;

private:
	PerfectWorld::SchoolSessionsState& Sessions;
	PerfectWorld::DayLoopState& Day;
	const PerfectWorld::SchoolSessionCatalogues& Catalogues;
	PerfectWorld::SchoolLessonBinding Binding;
	int32 SkillMilli = 0;
	uint64 WorldSeed = 0;
	std::vector<PerfectWorld::ReceiptCandidate> KnownAlternatives;
};

// ---- What the screen shows of the period and the run, read through the bridge ----

struct FFrame
{
	PerfectWorld::SchoolLessonBinding Binding;
	PerfectWorld::SchoolPeriodView Period;
	PerfectWorld::SchoolLessonView View;
	/** Minutes in the bell period (45); a leave-early charge is capped one minute below it. */
	int32 PeriodMinutes = 45;

	/** A run exists. */
	bool bHasRun = false;
	/** The run belongs to a different binding: this screen cannot drive it. */
	bool bOtherLesson = false;
	bool bReadyToClose = false;
	uint64 Session = 0;
	PerfectWorld::SchoolLessonMode RunMode = PerfectWorld::SchoolLessonMode::Attend;
	PerfectWorld::SchoolLessonOutcome Outcome;

	/** The structure's exposure is running (false once the exercise has finished, or for routine and attend). */
	bool bExposureActive = false;
	int32 UnitsLeft = 0;
	int32 UnitsTotal = 0;

	/** Resolved through IBridge::ResolveText; empty when the key is unknown. */
	FText Title;
	FText Brief;
	FText Role;
};

/** Reads the frame through the bridge. Units are filled by the shell, which knows the structure's exposure. */
FFrame BuildFrame(const IBridge& Bridge);

/** How far a family is mastered, as the core's own record says. */
enum class EMastery : uint8 { Unseen, Practised, Secure };

/** The pieces of a finished or routine outcome that every structure shares. */
struct FOutcomeFacts
{
	PerfectWorld::StructureGrade Grade = PerfectWorld::StructureGrade::Sound;
	bool bForeground = true;
	int32 LearningPermille = 0;
	int32 UnitsSpent = 0;
	bool bBecameSecure = false;
	/** Sentences for the failure bits the structure reports. */
	TArray<FText> Diagnostics;
	/** Structure-specific detail lines (links left out, items placed, what was named ...). */
	TArray<FText> Detail;
};

/** What a screen needs from its host. */
struct FShellArgs
{
	TSharedPtr<IBridge> Bridge;
	TAttribute<FVector2D> Space;
	FSimpleDelegate OnClose;
	FPWClassLessonClosed OnLessonClosed;
	TWeakPtr<SWidget> ReturnFocus;
};
} // namespace PWClassLesson

/**
 * The shared lesson screen. Idle: choose how to take the lesson (work the exercise, use routine, sit through it) and a goal, then start.
 * Playing: the structure's case context and choices, two panes when there is room. Finished: the typed outcome and the Record button.
 * Closed: a summary of what the core recorded. Leave early charges the working time spent and earns no learning.
 */
class SPWClassLesson : public SCompoundWidget
{
public:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) override;

	/** Re-reads the period and run through the bridge and rebuilds the body, keeping the focused action where it can. Call after anything
	 *  outside the screen changed the session. */
	void RefreshModel();
	/** Re-checks density and pane arrangement against the space now; rebuilds and returns true when it changed. */
	bool RefreshLayout();
	/** True while case context and choices are in two panes. */
	bool IsSplit() const { return bBuiltSplit; }
	FVector2D GetSpace() const;
	const PWClassLesson::FFrame& GetFrame() const { return Frame; }

	PerfectWorld::SchoolLessonMode GetMode() const { return Mode; }
	int32 GetGoalIndex() const { return GoalIndex; }
	/** Changes how the lesson is taken; ignored when that mode is not available now or the lesson has started. Returns whether it changed. */
	bool SetMode(PerfectWorld::SchoolLessonMode InMode);
	bool SetGoalIndex(int32 Index);
	void CycleMode(int32 Delta);
	void CycleGoal(int32 Delta);

	/** school.lesson.open in the selected mode and goal. A refusal changes nothing and is shown in words. */
	PerfectWorld::SchoolSessionResult ExecuteOpen();
	PerfectWorld::SchoolSessionResult ExecuteOpen(PerfectWorld::SchoolLessonMode InMode);
	/** school.lesson.close: records the period (false; needs a finished lesson) or leaves early (true). Fires OnLessonClosed once, only
	 *  when the core accepted. Leaving early reports the working time spent (units x ModelUnitMinutes, held under the period length). */
	PerfectWorld::SchoolSessionResult ExecuteClose(bool bCancel);
	/** The Leave early button: the first press arms it, the second calls ExecuteClose(true). Returns whether the lesson was left. */
	bool PressLeaveEarly();
	bool IsLeaveArmed() const { return bLeaveArmed; }
	bool CanLeaveEarly() const;

	const FText& GetBanner() const { return Banner; }
	bool IsBannerError() const { return bBannerError; }
	/** The core's last answer to a command from this screen (events, session, charge). */
	const PerfectWorld::SchoolSessionResult& GetLastResult() const { return LastResult; }
	FString GetLastIssueCode() const { return LastIssueCode; }
	/** The session this screen last showed (0 when none); every choose and close carries it. */
	uint64 GetDisplayedSession() const { return DisplayedSession; }
	/** The core accepted Record or Leave early through this screen; the summary is shown. */
	bool IsLessonClosed() const { return bLessonClosed; }

	/** The keys of the actions on screen, top to bottom (tests, automation). */
	TArray<FString> GetActionKeys() const;
	bool HasAction(const FString& Key) const;
	bool IsActionEnabled(const FString& Key) const;
	/** Runs the same handler the button would. False when there is no such enabled action. */
	bool PressAction(const FString& Key);

	/** Leaves the screen without changing the lesson: OnClose, then keyboard focus returns to the widget that opened it. */
	void RequestClose();
	/** Puts keyboard focus on the first action (or Back) once the layout can take it. */
	void FocusInitial();

protected:
	void ConstructShell(const PWClassLesson::FShellArgs& Args);

	// ---- The structure supplies these ----
	virtual PerfectWorld::SchoolStructure ShellStructure() const = 0;
	/** "phase2.a2.model." : the shell reads <prefix>eyebrow, <prefix>title and <prefix>subtitle. */
	virtual const TCHAR* ShellTextPrefix() const = 0;
	virtual int32 ShellGoalCount() const = 0;
	virtual FText ShellGoalName(int32 Index) const = 0;
	virtual FText ShellGoalHint(int32 Index) const { return FText::GetEmpty(); }
	virtual PerfectWorld::SchoolLessonGoal ShellGoalAt(int32 Index) const = 0;
	/** The running exposure of this structure. */
	virtual void ShellReadExposure(const PerfectWorld::SchoolLessonView& View, bool& bActive, int32& Left, int32& Total) const = 0;
	virtual PWClassLesson::EMastery ShellMastery(const PerfectWorld::SchoolLessonView& View, const std::string& FamilyId) const = 0;
	/** The session id changed (including to none): drop widget-local selections and drafts. Called before ShellFrameChanged. */
	virtual void ShellSessionChanged() {}
	/** Called after every refresh, before the body is built: rebuild the structure's own model from the frame. */
	virtual void ShellFrameChanged() {}
	/** The case context (left pane when split): name, brief, badges, the units meter, what is known so far. No actions. */
	virtual void ShellBuildCase(const TSharedRef<SVerticalBox>& Into) = 0;
	/** The choices of the current step (right pane when split): rows added with AddActionRow / AddQuotedRow. */
	virtual void ShellBuildPlay(const TSharedRef<SVerticalBox>& Into) = 0;
	/** Fills the shared facts of a typed outcome; false when the outcome is not this structure's. */
	virtual bool ShellDescribeOutcome(const PerfectWorld::SchoolLessonOutcome& Outcome, PWClassLesson::FOutcomeFacts& OutFacts) const = 0;

	// ---- Helpers for the structure's body ----
	bool HasBridge() const { return Bridge.IsValid(); }
	const PWClassLesson::IBridge& GetBridge() const { return *Bridge; }
	bool IsCompactNow() const;
	int32 TextSize(int32 Regular, int32 CompactSize) const { return IsCompactNow() ? CompactSize : Regular; }
	/** The core's quote for an action of the session this screen shows. */
	PerfectWorld::SchoolLessonQuote Quote(const PerfectWorld::SchoolLessonAction& Action) const;
	/** Sends one structure decision (school.lesson.choose with the displayed session). The core quotes again and refuses what it must.
	 *  Notes the result in the banner and refreshes. SuccessLine may be empty. */
	PerfectWorld::SchoolSessionResult ChooseAction(const PerfectWorld::SchoolLessonAction& Action, const FText& SuccessLine);
	/** Shows a message without touching the lesson. */
	void ShowBanner(const FText& Text, bool bError);

	enum class ECostTone : uint8 { Plain, Free, Short };
	/** Adds one pressable row (title, detail, price) to Into and registers it as an action. */
	void AddActionRow(const TSharedRef<SVerticalBox>& Into, const FString& Key, const FText& Title, const FText& Detail, const FText& Cost,
		ECostTone Tone, bool bEnabled, TFunction<void()> Handler, bool bSelected = false);
	/** A row whose price is the core's quote: disabled, with the refusal in words, when the quote is refused; while bReady is false the row
	 *  is disabled with NotReadyHint (nothing chosen yet) and shows no price. */
	void AddQuotedRow(const TSharedRef<SVerticalBox>& Into, const FString& Key, const FText& Title, const FText& Detail,
		const PerfectWorld::SchoolLessonQuote& Quote, TFunction<void()> Handler, bool bSelected = false, bool bReady = true,
		const FText& NotReadyHint = FText());
	/** Adds the one saffron call to action of the current state (Start, Record) and registers it as an action. */
	void AddPrimaryAction(const TSharedRef<SVerticalBox>& Into, const FString& Key, const FText& Label, TFunction<void()> Handler);
	void AddHeading(const TSharedRef<SVerticalBox>& Into, const FText& Text);
	void AddNote(const TSharedRef<SVerticalBox>& Into, const FText& Text, bool bMuted = true);
	void AddLine(const TSharedRef<SVerticalBox>& Into, const FText& Text, const FLinearColor& Color, bool bBold = false);
	void AddUnitsMeter(const TSharedRef<SVerticalBox>& Into);

private:
	struct FActionSlot
	{
		FString Key;
		TSharedPtr<SWidget> Widget;
		bool bEnabled = true;
		TFunction<void()> Handler;
	};
	enum class ELayout : uint8 { Single, Split };

	// SPWClassLesson.cpp
	void ClampSelections();
	void RestoreFocusAfterRebuild(int32 PreviousIndex, bool bHadFocus);
	int32 FocusedActionIndex() const;
	int32 LeaveMinutes() const;
	bool IsWrongScreen() const;
	bool WantsSplit() const;
	void ScrollBy(float Delta);
	PerfectWorld::SchoolLessonGoal CurrentGoal() const { return ShellGoalAt(GoalIndex); }
	FText PeriodLine() const;

	// SPWClassLessonBody.cpp
	void RebuildBody();
	void SetLayout(ELayout InLayout);
	void BuildIdle(const TSharedRef<SVerticalBox>& Into);
	void BuildIntro(const TSharedRef<SVerticalBox>& Into);
	void BuildFinished(const TSharedRef<SVerticalBox>& Into);
	void BuildSummary(const TSharedRef<SVerticalBox>& Into);
	FText FamilyName() const;

	TSharedPtr<PWClassLesson::IBridge> Bridge;
	TAttribute<FVector2D> Space;
	FSimpleDelegate OnClose;
	FPWClassLessonClosed OnLessonClosed;
	TWeakPtr<SWidget> ReturnFocus;

	PWClassLesson::FFrame Frame;
	PerfectWorld::SchoolLessonMode Mode = PerfectWorld::SchoolLessonMode::Foreground;
	int32 GoalIndex = 0;
	uint64 DisplayedSession = 0;
	PerfectWorld::SchoolSessionResult LastResult;
	FString LastIssueCode;
	FText Banner;
	bool bBannerError = false;
	bool bLeaveArmed = false;
	bool bWantFocus = true;
	bool bBuiltCompact = false;
	bool bBuiltSplit = false;
	/** False until the first refresh in ConstructShell has run (so a lesson already open is announced as resumed). */
	bool bStarted = false;

	// The summary after a close (a snapshot taken just before the close).
	bool bLessonClosed = false;
	bool bClosedByLeaving = false;
	PerfectWorld::SchoolLessonOutcome ClosedOutcome;
	PerfectWorld::SchoolLessonMode ClosedMode = PerfectWorld::SchoolLessonMode::Attend;
	int32 ClosedMinutes = 0;
	int32 ClosedMilliXp = 0;

	TArray<FActionSlot> Actions;
	ELayout BuiltLayout = ELayout::Single;
	TSharedPtr<SBox> BodyHolder;
	TSharedPtr<SVerticalBox> CaseBox;
	TSharedPtr<SVerticalBox> PlayBox;
	TSharedPtr<SScrollBox> PlayScroll;
	TSharedPtr<SScrollBox> SideScroll;
	TWeakPtr<SWidget> BackButton;
};
