#pragma once

#include "CoreMinimal.h"
#include "Framework/SlateDelegates.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "SchoolSessions.h"
#include "StructureExplain.h"
#include "StructureModel.h"

class SBox;
class SScrollBox;
class SVerticalBox;

/**
 * Order A2: the two classroom lesson screens for the "model a system" (SPWClassModel) and "construct an explanation" (SPWClassExplain)
 * structures. They share the chrome in SPWClassLessonBase and the helpers in PWClassLesson (this header and SPWClassModel.cpp).
 *
 * Authority. The screens hold no rules. Every price on screen is a core quote (QuoteSchoolLessonAction), every charge is the core's own
 * ChargedUnits, every grade and diagnostic is the core's typed outcome, and the only things a screen sends are the open / choose /
 * close commands of SchoolSessions.h (school.lesson.open / .choose / .close) through IBridge. A screen never computes a mark, a
 * mastery state or a learning amount, never reads a solution flag (ModelLink::Sound, ModelCaseDef::Fault, ExplainClaim::Defensible,
 * ExplainEvidence::Supports/Against, ExplainLimitation::Bounds) and never calls a *StandardAction policy. The skill level, world seed
 * and known alternatives that OpenSchoolLesson needs are owned by the bridge, never by a widget.
 *
 * Keys while a lesson screen has focus: Esc, B and the controller menu button leave the screen WITHOUT cancelling the lesson (the
 * session is core state and can be resumed); Page Up, Page Down and the right stick scroll; Up, Down and the D-pad move between
 * buttons through Slate navigation; Enter or A presses the focused button. Cancelling is its own button and asks twice.
 * No motion is used, so reduced motion is respected by construction.
 */
namespace PWClassLesson
{
inline FVector2D SafeSpace(FVector2D Space)
{
	return FVector2D(FMath::Max(Space.X, 320.0), FMath::Max(Space.Y, 240.0));
}

/** The compact density: short or narrow logical spaces (853x480 at 150% text is about 569x320 logical). */
inline bool Compact(FVector2D Space)
{
	const FVector2D S = SafeSpace(Space);
	return S.X < 1160.0 || S.Y < 660.0;
}

inline bool TwoPane(FVector2D Space) { return !Compact(Space); }

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
inline const TCHAR* const ModeCardTag = TEXT("PWClassLessonModeCard");
inline const TCHAR* const ChoiceTag = TEXT("PWClassLessonChoice");
inline const TCHAR* const OutcomeTag = TEXT("PWClassLessonOutcome");
inline const TCHAR* const SummaryTag = TEXT("PWClassLessonSummary");
inline const TCHAR* const StatusTag = TEXT("PWClassLessonStatus");
inline const TCHAR* const NoteTag = TEXT("PWClassLessonNote");

// ---- Words (pure; English until the text pipeline loads Data/Text) -------------------

/** English for a text key of Data/Text/en/phase2_a2.json. False, with OutText untouched, for an unknown key. */
bool FindText(const FString& Key, FText& OutText);
/** English for a known key, else a neutral sentence. A key is never shown. */
FText Text(const TCHAR* Key);
FText Text(const FString& Key);
FText Text1(const TCHAR* Key, const FText& A);
FText Text2(const TCHAR* Key, const FText& A, const FText& B);
FText Text3(const TCHAR* Key, const FText& A, const FText& B, const FText& C);
FText Number(int64 Value);
/** Core and content strings are UTF-8. */
FText FromUtf8(const std::string& Value);
/** The sentence for a refusal code (school.session.*, model.*, structure.*, dayloop.*); neutral for an unknown code. */
FText IssueText(const std::string& Code);
/** "Cost: 2 u (After: 3 u)", "Cost: 0 u (After: 5 u)", or "Unavailable (reason)". Reads only the core's quote. */
FText QuoteText(const PerfectWorld::SchoolLessonQuote& Quote);
/** Every key of the English table, for the key-drift test. */
TArray<FString> KnownTextKeys();

// ---- The seam to the world --------------------------------------------------------------

/**
 * What a lesson screen needs from the world, and nothing else. The session id travels with every command exactly as the frozen
 * contract requires, so a stale screen is refused by the core, not by the widget. The integrator's implementation submits
 * school.lesson.open / .choose / .close through the reducer; FCoreBridge below is the interim bridge over plain core state.
 */
class IBridge
{
public:
	virtual ~IBridge() = default;

	virtual const PerfectWorld::SchoolLessonBinding& GetBinding() const = 0;
	virtual const PerfectWorld::SchoolSessionCatalogues& GetCatalogues() const = 0;
	virtual PerfectWorld::SchoolPeriodView QueryPeriod() const = 0;
	virtual PerfectWorld::SchoolLessonView QueryLesson() const = 0;
	/** school.lesson.open. The skill, seed and known alternatives come from the bridge's authorities, never a widget. */
	virtual PerfectWorld::SchoolSessionResult OpenLesson(PerfectWorld::SchoolLessonMode Mode, const PerfectWorld::SchoolLessonGoal& Goal) = 0;
	virtual PerfectWorld::SchoolLessonQuote QuoteAction(uint64 Session, const PerfectWorld::SchoolLessonAction& Action) const = 0;
	/** school.lesson.choose. */
	virtual PerfectWorld::SchoolSessionResult ChooseAction(uint64 Session, const PerfectWorld::SchoolLessonAction& Action) = 0;
	/** school.lesson.close: ElapsedMinutes is 0 unless bCancel. */
	virtual PerfectWorld::SchoolSessionResult CloseLesson(uint64 Session, bool bCancel, int32 ElapsedMinutes) = 0;
};

/** The interim bridge: the portable calls themselves, over state and catalogues the caller owns (they must outlive the bridge). */
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
	/** Diagnostic sentences for the failure bits the structure reports. */
	TArray<FText> Diagnostics;
	/** Structure-specific detail lines (links left out, items placed ...). */
	TArray<FText> Detail;
};

// ---- Shared widget builders -------------------------------------------------------------

/** A quiet inset note (the Tag is for tests). */
TSharedRef<SWidget> MakeNote(const FText& Text, const TCHAR* Tag = NoteTag);
/** A section heading in the accent colour. */
TSharedRef<SWidget> MakeHeading(const FText& Text);
/** Pips for the units left of a total. */
TSharedRef<SWidget> MakeUnitsPips(int32 Left, int32 Total);
/** The grade, diagnostics and detail lines of one outcome. */
TSharedRef<SWidget> MakeOutcomeBlock(const FOutcomeFacts& Facts, const TCHAR* Tag);
} // namespace PWClassLesson

/**
 * The part of a lesson screen both structures share: header, period line, the three ways to take the period (foreground, routine,
 * attend), the live beat area the structure fills in, the outcome and close step, the cancel step, the footer and the keys. It reads
 * the world only through PWClassLesson::IBridge.
 */
class SPWClassLessonBase : public SCompoundWidget
{
public:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void SetBridge(TSharedPtr<PWClassLesson::IBridge> InBridge);
	TSharedPtr<PWClassLesson::IBridge> GetBridge() const { return Bridge; }
	/** Re-reads the session through the bridge and rebuilds the screen. Call after anything outside the screen changed it. */
	void RefreshFromSession();
	/** Re-checks the pane arrangement against the space now; returns true when it changed. */
	bool RefreshLayout();
	bool IsTwoPane() const { return bTwoPane; }
	FVector2D GetSpace() const { return Space.Get(); }

	/** school.lesson.open for the chosen goal. Returns the core's answer; a refusal changes nothing. */
	PerfectWorld::SchoolSessionResult TriggerOpenLesson(PerfectWorld::SchoolLessonMode Mode);
	/** school.lesson.close, normal. Needs a finished lesson (the core refuses otherwise). */
	PerfectWorld::SchoolSessionResult TriggerCloseLesson();
	/** school.lesson.close with cancel. ElapsedMinutes below zero asks the screen to report the working time spent so far (units x 5 min,
	 *  held under the period length); the core validates and charges it. */
	PerfectWorld::SchoolSessionResult TriggerCancelLesson(int32 ElapsedMinutes = -1);
	/** Leaves the screen without touching the session, then hands focus back to the control that opened it. */
	void RequestLeavePanel();

	const PerfectWorld::SchoolSessionResult& GetLastResult() const { return LastResult; }
	FString GetLastIssueCode() const { return LastIssueCode; }
	FString GetLastStatusText() const { return LastStatus.ToString(); }
	/** The session this screen last showed (0 when none); every command carries it. */
	uint64 GetDisplayedSession() const { return DisplayedSession; }
	bool IsSummaryShown() const { return bShowSummary; }
	bool IsCancelArmed() const { return bCancelArmed; }

protected:
	struct FLessonInit
	{
		TSharedPtr<PWClassLesson::IBridge> Bridge;
		TAttribute<FVector2D> Space;
		TWeakPtr<SWidget> ReturnFocus;
		FSimpleDelegate OnClosed;
	};
	void InitLesson(const FLessonInit& Init);
	void Rebuild();

	// What a structure supplies.
	virtual PerfectWorld::SchoolLessonGoal CurrentGoal() const = 0;
	virtual void ResetSelections() = 0;
	virtual const TCHAR* TextPrefix() const = 0;
	virtual PerfectWorld::SchoolStructure Structure() const = 0;
	virtual PWClassLesson::EMastery MasteryOf(const PerfectWorld::SchoolLessonView& View, const std::string& FamilyId) const = 0;
	virtual bool ExposureActive(const PerfectWorld::SchoolLessonView& View) const = 0;
	virtual int32 UnitsLeft(const PerfectWorld::SchoolLessonView& View) const = 0;
	virtual int32 UnitsTotal(const PerfectWorld::SchoolLessonView& View) const = 0;
	virtual TSharedRef<SWidget> BuildGoalPicker() = 0;
	virtual TSharedRef<SWidget> BuildCasePane(const PerfectWorld::SchoolLessonView& View) = 0;
	virtual TSharedRef<SWidget> BuildBeatPane(const PerfectWorld::SchoolLessonView& View) = 0;
	virtual bool DescribeOutcome(const PerfectWorld::SchoolLessonOutcome& Outcome, PWClassLesson::FOutcomeFacts& OutFacts) const = 0;

	// What a structure uses.
	/** school.lesson.choose with the session this screen is showing. Updates the status line and rebuilds on success. */
	PerfectWorld::SchoolSessionResult RunChoose(const PerfectWorld::SchoolLessonAction& Action, const TCHAR* SuccessKey);
	PerfectWorld::SchoolLessonQuote Quote(const PerfectWorld::SchoolLessonAction& Action) const;
	void SetStatus(const FText& Text, const FString& IssueCode);
	/** A selectable row with its price beside it. */
	TSharedRef<SWidget> MakeChoiceRow(const FText& Label, TAttribute<bool> Selected, TAttribute<bool> Enabled, FOnClicked OnClicked,
		TAttribute<FText> Price);
	/** A commit button with its price beside it. */
	TSharedRef<SWidget> MakeCommitRow(const FText& Label, TAttribute<bool> Enabled, FOnClicked OnClicked, TAttribute<FText> Price);
	void RegisterFocus(const TSharedRef<SWidget>& Target);

	TSharedPtr<PWClassLesson::IBridge> Bridge;
	TAttribute<FVector2D> Space;
	TWeakPtr<SWidget> ReturnFocus;
	FSimpleDelegate OnClosed;

	uint64 DisplayedSession = 0;
	PerfectWorld::SchoolSessionResult LastResult;
	FString LastIssueCode;
	FText LastStatus;

private:
	TSharedRef<SWidget> BuildHeader(const PerfectWorld::SchoolPeriodView& Period);
	TSharedRef<SWidget> BuildIntro(const PerfectWorld::SchoolPeriodView& Period, const PerfectWorld::SchoolLessonView& View);
	TSharedRef<SWidget> BuildModeCards(const PerfectWorld::SchoolPeriodView& Period);
	TSharedRef<SWidget> BuildModeCard(const FText& Title, const FText& Detail, const FText& ButtonLabel, bool bEnabled,
		const FText& WhyNot, PerfectWorld::SchoolLessonMode Mode);
	TSharedRef<SWidget> BuildRunBody(const PerfectWorld::SchoolLessonView& View);
	TSharedRef<SWidget> BuildOutcomePane(const PerfectWorld::SchoolLessonRun& Run);
	TSharedRef<SWidget> BuildSummaryPane();
	TSharedRef<SWidget> BuildFooter(const PerfectWorld::SchoolLessonView& View);
	int32 ElapsedSoFar(const PerfectWorld::SchoolLessonView& View) const;
	void ScrollBy(float Delta);
	FText PeriodLine(const PerfectWorld::SchoolPeriodView& Period) const;

	TSharedPtr<SBox> BodyHolder;
	TSharedPtr<SScrollBox> MainScroll;
	TSharedPtr<SScrollBox> SideScroll;
	TWeakPtr<SWidget> FocusTarget;
	bool bTwoPane = false;
	bool bShowSummary = false;
	bool bCancelArmed = false;

	// What the summary after a close shows (a snapshot taken just before the close).
	PerfectWorld::SchoolLessonOutcome ClosedOutcome;
	PerfectWorld::SchoolLessonMode ClosedMode = PerfectWorld::SchoolLessonMode::Attend;
	bool bClosedCancelled = false;
	int32 ClosedMinutes = 0;
	int32 ClosedMilliXp = 0;
};

/** The model-a-system lesson: quantities and links, the discrepancy check, the claim. */
class SPWClassModel : public SPWClassLessonBase
{
public:
	SLATE_BEGIN_ARGS(SPWClassModel) : _Space(FVector2D(1920.0, 1080.0)) {}
		SLATE_ARGUMENT(TSharedPtr<PWClassLesson::IBridge>, Bridge)
		/** The logical size of the screen the panel may use. The host passes the size it already measures. */
		SLATE_ATTRIBUTE(FVector2D, Space)
		/** The control that opened the lesson; it gets keyboard focus back when the screen closes. Held weakly. */
		SLATE_ARGUMENT(TWeakPtr<SWidget>, ReturnFocusTarget)
		SLATE_EVENT(FSimpleDelegate, OnClosed)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SelectGoal(PerfectWorld::ModelGoal InGoal);
	PerfectWorld::ModelGoal GetSelectedGoal() const { return SelectedGoal; }

	/** The worked example, when the method is unfamiliar (beat 1). */
	PerfectWorld::SchoolSessionResult TriggerWorkedExample();
	void ToggleLink(const FString& LinkId);
	bool IsLinkSelected(const FString& LinkId) const { return SelectedLinks.Contains(LinkId); }
	const TArray<FString>& GetSelectedLinks() const { return SelectedLinks; }
	PerfectWorld::SchoolSessionResult TriggerCommitModel();
	void SelectInvestigation(PerfectWorld::ModelInvestigation InInvestigation);
	PerfectWorld::ModelInvestigation GetSelectedInvestigation() const { return SelectedInvestigation; }
	PerfectWorld::SchoolSessionResult TriggerCommitInvestigation();
	void SelectClaim(PerfectWorld::ModelClaim InClaim);
	bool HasSelectedClaim() const { return bClaimChosen; }
	PerfectWorld::ModelClaim GetSelectedClaim() const { return SelectedClaim; }
	void SetExplain(bool bInExplain);
	bool GetExplain() const { return bExplain; }
	PerfectWorld::SchoolSessionResult TriggerPresentClaim();

protected:
	virtual PerfectWorld::SchoolLessonGoal CurrentGoal() const override { return SelectedGoal; }
	virtual void ResetSelections() override;
	virtual const TCHAR* TextPrefix() const override { return TEXT("phase2.a2.model."); }
	virtual PerfectWorld::SchoolStructure Structure() const override { return PerfectWorld::SchoolStructure::Model; }
	virtual PWClassLesson::EMastery MasteryOf(const PerfectWorld::SchoolLessonView& View, const std::string& FamilyId) const override;
	virtual bool ExposureActive(const PerfectWorld::SchoolLessonView& View) const override { return View.Model.Exposure.Active(); }
	virtual int32 UnitsLeft(const PerfectWorld::SchoolLessonView& View) const override { return View.Model.Exposure.UnitsLeft; }
	virtual int32 UnitsTotal(const PerfectWorld::SchoolLessonView& View) const override { return View.Model.Exposure.UnitsTotal; }
	virtual TSharedRef<SWidget> BuildGoalPicker() override;
	virtual TSharedRef<SWidget> BuildCasePane(const PerfectWorld::SchoolLessonView& View) override;
	virtual TSharedRef<SWidget> BuildBeatPane(const PerfectWorld::SchoolLessonView& View) override;
	virtual bool DescribeOutcome(const PerfectWorld::SchoolLessonOutcome& Outcome, PWClassLesson::FOutcomeFacts& OutFacts) const override;

private:
	const PerfectWorld::ModelCaseDef* ActiveCase(const PerfectWorld::SchoolLessonView& View) const;
	PerfectWorld::ModelAction CommitAction() const;
	PerfectWorld::ModelAction InvestigateActionNow() const;
	PerfectWorld::ModelAction ClaimAction() const;

	PerfectWorld::ModelGoal SelectedGoal = PerfectWorld::ModelGoal::UnderstandModel;
	TArray<FString> SelectedLinks;
	PerfectWorld::ModelInvestigation SelectedInvestigation = PerfectWorld::ModelInvestigation::None;
	PerfectWorld::ModelClaim SelectedClaim = PerfectWorld::ModelClaim::BoundedRange;
	bool bClaimChosen = false;
	bool bExplain = false;
	bool bExplainTouched = false;
};
