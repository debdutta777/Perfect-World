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
 * Order A3 shared lesson shell (prefix A3 / PWClassA3), used by SPWClassInvestigate and SPWClassCoordinate.
 *
 * These screens hold no rules. Every number, price, grade and refusal is the portable SchoolSessions contract (P2C v1): the screen asks
 * a bridge to query the period and lesson, to QUOTE a decision before it is committed, to open, to choose and to close. It never reads a
 * solution flag for display (InvestigateCause::Fault, CoordinateFact::Critical, CoordinateCaseDef::Constraint and ::Seam), never awards
 * marks and never supplies skill. A quote is always the price shown; a refusal is shown in words and changes nothing.
 */
namespace PWClassA3
{
// ---- Layout (logical units, the same density switch as the pause menu's windows) ----

bool Compact(float Width, float Height);
float PanelWidth(float Width, float Height);
float PanelMaxHeight(float Width, float Height);

// ---- Words. All text is keyed "phase2.a3.*" (Data/Text/en/phase2_a3.json); an unknown key reads as empty, never as the key ----

/** The English for a phase2.a3.* key, or empty text when the key is unknown. */
FText Str(const TCHAR* Key);
FText Str(const FString& Key);
FString FromUtf8(const std::string& Text);
FText TextFromUtf8(const std::string& Text);
/** "09:35" from a minute of the day. */
FText ClockText(int32 MinuteOfDay);
/** Words for a core refusal code (school.session.*, structure.*); neutral text for an unknown or empty code. */
FText IssueText(const PerfectWorld::SchoolIssue& Issue);
FText ModeText(PerfectWorld::SchoolLessonMode Mode);
FText ModeHint(PerfectWorld::SchoolLessonMode Mode);
FText GradeText(PerfectWorld::StructureGrade Grade);
/** "Free", "1 unit" or "3 units" for a quoted price. */
FText CostText(int32 Units);
/** "Time left: 4 of 7 units". */
FText UnitsText(int32 Left, int32 Total);

inline const TCHAR* const PanelTag = TEXT("PWClassA3Panel");
inline const TCHAR* const RowTag = TEXT("PWClassA3Row");
inline const TCHAR* const BannerTag = TEXT("PWClassA3Banner");

// ---- The bridge: everything the screens need from the world, in SchoolSessions' own terms ----

/**
 * One lesson binding's session seen from the screen. The integrator's implementation submits the school.lesson.* commands to the
 * reducer (one atomic transaction each); FCoreLessonBridge calls the contract functions directly over borrowed state, for tests and
 * for a standalone harness. Skill, seed and known alternatives belong to the bridge, never to a widget.
 */
class ILessonBridge
{
public:
    virtual ~ILessonBridge() = default;

    virtual const PerfectWorld::SchoolLessonBinding& GetBinding() const = 0;
    virtual const PerfectWorld::SchoolSessionCatalogues& GetCatalogues() const = 0;
    virtual PerfectWorld::SchoolPeriodView QueryPeriod() const = 0;
    virtual PerfectWorld::SchoolLessonView QueryLesson() const = 0;
    virtual PerfectWorld::SchoolSessionResult OpenLesson(PerfectWorld::SchoolLessonMode Mode, const PerfectWorld::SchoolLessonGoal& Goal) = 0;
    virtual PerfectWorld::SchoolLessonQuote QuoteAction(uint64 Session, const PerfectWorld::SchoolLessonAction& Action) const = 0;
    virtual PerfectWorld::SchoolSessionResult ChooseAction(uint64 Session, const PerfectWorld::SchoolLessonAction& Action) = 0;
    virtual PerfectWorld::SchoolSessionResult CloseLesson(uint64 Session, bool bCancel, int32 ElapsedMinutes) = 0;
    /** The words for one of the binding's text keys (A1 owns them); empty when unknown, and the screen then uses neutral wording. */
    virtual FText ResolveText(const FString& Key) const { return Str(Key); }
};

/** Direct adapter over SchoolSessionsState + DayLoopState + the borrowed catalogues. The state must outlive it. */
class FCoreLessonBridge final : public ILessonBridge
{
public:
    FCoreLessonBridge(PerfectWorld::SchoolSessionsState& InSessions, PerfectWorld::DayLoopState& InDay,
                      const PerfectWorld::SchoolSessionCatalogues& InCatalogues, PerfectWorld::SchoolLessonBinding InBinding,
                      int32 InSkillMilli, uint64 InWorldSeed, std::vector<PerfectWorld::ReceiptCandidate> InAlternatives = {});

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
    std::vector<PerfectWorld::ReceiptCandidate> Alternatives;
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

    FText Title;
    FText Brief;
    FText Role;
};

/** Reads the frame through the bridge for the structure a screen drives. Units are filled by the screen (it knows its exposure). */
FFrame BuildFrame(const ILessonBridge& Bridge);

/** What a screen needs from its host, and the arguments the shell keeps. */
struct FShellArgs
{
    TSharedPtr<ILessonBridge> Bridge;
    TAttribute<FVector2D> Space;
    FSimpleDelegate OnClose;
    FPWClassLessonClosed OnLessonClosed;
    TWeakPtr<SWidget> ReturnFocus;
};
} // namespace PWClassA3

/**
 * The shared lesson screen. Idle: choose how to take the lesson (work the exercise, use routine, sit through it) and a focus, then start.
 * Playing: the structure's own body. Finished: the typed outcome from the core and the paid Record button. Leave early: the core charges
 * the time spent. Nothing here is a rule.
 *
 * Every action is a real button, so keyboard and controller focus work through Slate's own navigation:
 *   Up, Down, D-pad, left stick  move between actions (the body scrolls to the focused one)
 *   Enter, A                     press the focused action
 *   G, right shoulder            change the focus of the exercise (before it starts)
 *   M, left shoulder             change how the lesson is taken (before it starts)
 *   Esc, B, menu button          go back (a running lesson stays open; come back to it, or choose Leave early)
 * No motion is used, so reduced motion is respected by construction.
 */
class SPWClassLessonShell : public SCompoundWidget
{
public:
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
    virtual void OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) override;

    /** Re-reads the period and run through the bridge and rebuilds the body; keeps the focused action where it can. */
    void RefreshModel();
    const PWClassA3::FFrame& GetFrame() const { return Frame; }

    PerfectWorld::SchoolLessonMode GetMode() const { return Mode; }
    int32 GetGoalIndex() const { return GoalIndex; }
    /** Changes how the lesson is taken; ignored when that mode is not available now or the lesson has started. Returns whether it changed. */
    bool SetMode(PerfectWorld::SchoolLessonMode InMode);
    bool SetGoalIndex(int32 Index);
    void CycleMode(int32 Delta);
    void CycleGoal(int32 Delta);

    PerfectWorld::SchoolSessionResult ExecuteOpen();
    /** Records the period (false) or leaves early (true). Fires OnLessonClosed once, only when the core accepted. */
    PerfectWorld::SchoolSessionResult ExecuteClose(bool bCancel);

    const FText& GetBanner() const { return Banner; }
    bool IsBannerError() const { return bBannerError; }
    bool IsLessonClosed() const { return bLessonClosed; }
    /** The keys of the actions on screen, top to bottom (tests, automation). */
    TArray<FString> GetActionKeys() const;
    bool HasAction(const FString& Key) const;
    bool IsActionEnabled(const FString& Key) const;
    /** Runs the same handler the button would. False when there is no such enabled action. */
    bool PressAction(const FString& Key);

    /** Leaves the screen without changing the lesson: OnClose, then focus returns to the argument's widget. */
    void RequestClose();
    /** Puts keyboard focus on the first action (or Back) once the layout can take it. */
    void FocusInitial();

protected:
    void ConstructShell(const PWClassA3::FShellArgs& Args);

    // ---- The structure supplies these ----
    virtual PerfectWorld::SchoolStructure ShellStructure() const = 0;
    virtual FText ShellSubtitle() const = 0;
    virtual int32 ShellGoalCount() const = 0;
    virtual FText ShellGoalName(int32 Index) const = 0;
    virtual FText ShellGoalHint(int32 Index) const = 0;
    virtual PerfectWorld::SchoolLessonGoal ShellGoalAt(int32 Index) const = 0;
    /** The units of the running exposure for this structure. */
    virtual void ShellReadExposure(const PerfectWorld::SchoolLessonView& View, bool& bActive, int32& Left, int32& Total) const = 0;
    /** Called after every refresh, before the body is built: rebuild the structure's own model. */
    virtual void ShellFrameChanged() = 0;
    virtual void ShellBuildPlay(const TSharedRef<SVerticalBox>& Into) = 0;
    /** Lines about a finished typed outcome (what was diagnosed, which failures), not including the grade and the time. */
    virtual TArray<FText> ShellOutcomeDetail(const PerfectWorld::SchoolLessonOutcome& Outcome) const = 0;

    // ---- Helpers for the structure's body ----
    bool HasBridge() const { return Bridge.IsValid(); }
    const PWClassA3::ILessonBridge& GetBridge() const { return *Bridge; }
    bool IsCompactNow() const;
    int32 TextSize(int32 Regular, int32 CompactSize) const { return IsCompactNow() ? CompactSize : Regular; }
    /** Runs one structure decision through the bridge (the core quotes again and refuses what it must), notes the result and refreshes. */
    PerfectWorld::SchoolSessionResult ChooseAction(const PerfectWorld::SchoolLessonAction& Action, const FText& SuccessLine);
    /** Shows a message without touching the lesson. */
    void ShowBanner(const FText& Text, bool bError);

    enum class ECostTone : uint8 { Plain, Free, Short };
    /** Adds one pressable row (title, detail, price) to Into and registers it as an action. */
    void AddActionRow(const TSharedRef<SVerticalBox>& Into, const FString& Key, const FText& Title, const FText& Detail, const FText& Cost,
                      ECostTone Tone, bool bEnabled, TFunction<void()> Handler, bool bSelected = false);
    /** Adds the one saffron call to action of the current state (Start, Record) and registers it as an action. */
    void AddPrimaryAction(const TSharedRef<SVerticalBox>& Into, const FString& Key, const FText& Label, TFunction<void()> Handler);
    void AddHeading(const TSharedRef<SVerticalBox>& Into, const FText& Text);
    void AddNote(const TSharedRef<SVerticalBox>& Into, const FText& Text, bool bMuted = true);
    void AddUnitsMeter(const TSharedRef<SVerticalBox>& Into);

private:
    struct FActionSlot
    {
        FString Key;
        TSharedPtr<SWidget> Widget;
        bool bEnabled = true;
        TFunction<void()> Handler;
    };

    FVector2D GetSpace() const;
    void RebuildBody();
    void BuildIdle(const TSharedRef<SVerticalBox>& Into);
    void BuildFinished(const TSharedRef<SVerticalBox>& Into);
    void BuildHeader(const TSharedRef<SVerticalBox>& Into);
    void ClampSelections();
    void RestoreFocusAfterRebuild(int32 PreviousIndex, bool bHadFocus);
    int32 FocusedActionIndex() const;
    int32 LeaveMinutes() const;
    bool CanLeaveEarly() const;
    PerfectWorld::SchoolLessonGoal CurrentGoal() const { return ShellGoalAt(GoalIndex); }

    TSharedPtr<PWClassA3::ILessonBridge> Bridge;
    TAttribute<FVector2D> Space;
    FSimpleDelegate OnClose;
    FPWClassLessonClosed OnLessonClosed;
    TWeakPtr<SWidget> ReturnFocus;

    PWClassA3::FFrame Frame;
    PerfectWorld::SchoolLessonMode Mode = PerfectWorld::SchoolLessonMode::Foreground;
    int32 GoalIndex = 0;
    FText Banner;
    bool bBannerError = false;
    bool bLessonClosed = false;
    bool bClosedByLeaving = false;
    bool bWantFocus = true;
    bool bBuiltCompact = false;

    TArray<FActionSlot> Actions;
    TSharedPtr<SVerticalBox> BodyBox;
    TSharedPtr<SScrollBox> BodyScroll;
    TWeakPtr<SWidget> BackButton;
};

namespace PWClassInvestigate
{
/** One diagnostic test as the player sees it. The result is only meaningful once the test has been run. */
struct FTestRow
{
    FString Id;
    FText Name;
    FText Kind;
    /** The causes the test would implicate, in words. */
    FText Implicates;
    /** The core's quote for this decision, right now. */
    int32 Units = 0;
    int32 UnitsAfter = 0;
    PerfectWorld::SchoolIssue Issue;
    bool bRun = false;
    bool bPositive = false;
};

/** One candidate cause. The real fault is never part of this row. */
struct FCauseRow
{
    FString Id;
    FText Name;
    bool bStanding = true;
    /** True when naming this cause now would be a guess (other causes still stand). */
    bool bGuess = false;
    PerfectWorld::SchoolIssue Issue;
};

struct FModel
{
    bool bActive = false;
    FString CaseId;
    FText CaseName;
    FText CaseBrief;
    FText Symptom;
    FText Role;
    FText Scarce;
    FText Goal;
    FText Method;
    int32 UnitsLeft = 0;
    int32 UnitsTotal = 0;
    int32 Standing = 0;
    TArray<FTestRow> Tests;
    TArray<FCauseRow> Causes;
    /** What the tests run so far have shown, in the order they were run. */
    TArray<FText> Evidence;
    PerfectWorld::SchoolIssue ReportIssue;
};

/** Reads the running exposure through the bridge's quotes. An idle or foreign run gives an inactive model. */
FModel BuildModel(const PWClassA3::ILessonBridge& Bridge, const PWClassA3::FFrame& Frame);
} // namespace PWClassInvestigate

/**
 * StructureInvestigate lesson: a loop of diagnostic tests with different prices and adaptive evidence, then one conclusion. The tests
 * show which causes they implicate and a result only after they were run; a free honest report is always available. Which cause is the
 * real fault is never shown.
 */
class SPWClassInvestigate : public SPWClassLessonShell
{
public:
    SLATE_BEGIN_ARGS(SPWClassInvestigate) : _Space(FVector2D(1920.0, 1080.0)) {}
        SLATE_ARGUMENT(TSharedPtr<PWClassA3::ILessonBridge>, Bridge)
        /** The logical size of the screen the panel may use. The pause menu passes the size it already measures. */
        SLATE_ATTRIBUTE(FVector2D, Space)
        SLATE_EVENT(FSimpleDelegate, OnClose)
        SLATE_EVENT(FPWClassLessonClosed, OnLessonClosed)
        /** Focus goes back here after OnClose. */
        SLATE_ARGUMENT(TWeakPtr<SWidget>, ReturnFocus)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    const PWClassInvestigate::FModel& GetModel() const { return Model; }
    PerfectWorld::SchoolSessionResult ExecuteRunTest(const FString& TestId);
    PerfectWorld::SchoolSessionResult ExecuteDiagnose(const FString& CauseId);
    PerfectWorld::SchoolSessionResult ExecuteReportInconclusive();

protected:
    virtual PerfectWorld::SchoolStructure ShellStructure() const override { return PerfectWorld::SchoolStructure::Investigate; }
    virtual FText ShellSubtitle() const override;
    virtual int32 ShellGoalCount() const override;
    virtual FText ShellGoalName(int32 Index) const override;
    virtual FText ShellGoalHint(int32 Index) const override;
    virtual PerfectWorld::SchoolLessonGoal ShellGoalAt(int32 Index) const override;
    virtual void ShellReadExposure(const PerfectWorld::SchoolLessonView& View, bool& bActive, int32& Left, int32& Total) const override;
    virtual void ShellFrameChanged() override;
    virtual void ShellBuildPlay(const TSharedRef<SVerticalBox>& Into) override;
    virtual TArray<FText> ShellOutcomeDetail(const PerfectWorld::SchoolLessonOutcome& Outcome) const override;

private:
    PWClassInvestigate::FModel Model;
};
