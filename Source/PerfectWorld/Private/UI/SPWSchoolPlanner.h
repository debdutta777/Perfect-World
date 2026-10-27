#pragma once

#include "CoreMinimal.h"
#include "OpportunityReceipts.h"
#include "SchoolWeek.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SScrollBox;

/**
 * Order A6: the week planner. It shows what a standing order will do before anything is spent, and what really happened afterwards.
 * It holds no rules: orders are stored and the week is resolved only through the frozen SchoolWeek commands (school.week.order,
 * school.week.resolve) behind IPWSchoolPlannerWorld, and every number or flag it prints comes from the portable SchoolWeekBuild /
 * SchoolWeekResult it is given.
 *
 * PAUSES COST NOTHING. Opening, reading, switching a draft, pressing Edit and closing never touch the bridge's clock. Only the one
 * "Resolve this week" button submits school.week.resolve; saving or removing an order submits school.week.order, which moves no clock.
 * FIRSTS INTERRUPT. The preview marks the entry compression will stop before (the plan's own FirstOfKind / NeedsChoice flags, which
 * BuildSchoolWeekPlan sets for new lessons, played-by-hand lessons, unmastered routine and nominated scene days).
 * NO RETROSPECTIVE RECEIPT. The recap is built only from a SchoolWeekResult that really resolved in this session (Segments, the raw
 * ActivityEvents and the real DayLoop receipts they point at). It is session state: after a reload it is empty and says so; nothing is
 * rebuilt from attendance or from the plan.
 *
 * Layout: one screen-aligned column that scrolls inside the card, so it fits 853x480 to 1920x1080 and enlarged text.
 * Keys: Esc, B, the menu button and the left shoulder go back; PageUp/PageDown and the right stick scroll; Tab and the d-pad move focus.
 */

/** One alternative the player already knew about that an entry passes over. */
struct FPWDisplacedRow
{
    FText Name;
    int32 OverlapMinutes = 0;
};

struct FPWPlanEntryRow
{
    int32 Day = 0;
    int32 Entry = 0;
    FText Time;
    FText What;
    /** Why it is marked: standing order, "you play this", scene day, where compression stops. */
    TArray<FText> Notes;
    TArray<FPWDisplacedRow> Displaced;
    bool bDecidedByOrder = false;
    bool bYouPlay = false;
    bool bStopsHere = false;
};

struct FPWPlanDayRow
{
    int32 Day = 0;
    FText Heading;
    bool bSceneDay = false;
    TArray<FPWPlanEntryRow> Entries;
};

struct FPWOrderRow
{
    FString Id;
    FText Title;
    uint8 Weekdays = 0;
    bool bAttendSchool = true;
    PerfectWorld::SchoolLessonMode Mode = PerfectWorld::SchoolLessonMode::Attend;
};

struct FPWSceneDayRow
{
    int64 Day = 0;
    FText Date;
};

struct FPWRecapRow
{
    FText Time;
    FText What;
    TArray<FText> Notes;
};

/** What a resolved week really did. Present only when a SchoolWeekResult was stored this session. */
struct FPWRecapModel
{
    bool bPresent = false;
    FText Headline;
    FText Stop;
    FText Passed;
    FText Interrupts;
    TArray<FPWRecapRow> Rows;
};

struct FPWPlannerModel
{
    /** False when the world could not be read (no game world). */
    bool bAvailable = false;
    FText Clock;
    TArray<FPWOrderRow> Orders;
    TArray<FPWSceneDayRow> SceneDays;
    /** False when the saved orders and commitments cannot be built into a week; PlanProblem says why. */
    bool bPlanOk = false;
    FText PlanProblem;
    TArray<FPWPlanDayRow> Days;
    /** Where compression would stop, or that it would not. */
    FText StopSummary;
    bool bStopsAtAll = false;
    int32 EntryCount = 0;
    FPWRecapModel Recap;
};

/** The core's answer to a command the planner submitted. Code is empty when it was accepted. */
struct FPWPlannerOutcome
{
    bool bAccepted = false;
    FString Code;
};

/** What the planner reads for the next seven days: the saved orders and nominated days, the plan built from them, and what it needs to name things. */
struct FPWPlannerPreview
{
    PerfectWorld::SchoolWeekState Standing;
    /** BuildSchoolWeekPlan over Standing and the commitments already known; an Issue means the plan was refused. */
    PerfectWorld::SchoolWeekBuild Build;
    PerfectWorld::GameInstant Now = 0;
    /** Copies, so the preview owns what it names: activity durations and which lessons were really completed. */
    PerfectWorld::ActivityCatalogue Activities;
    PerfectWorld::SchoolSessionsState Sessions;
    /** Commitments the player already knows about (promises, appointments) as receipt candidates: the "known displaced commitments". */
    std::vector<PerfectWorld::ReceiptCandidate> Known;
};

/** school.week.resolve as the bridge saw it. Plan and Result are the actual ones; nothing is built afterwards. */
struct FPWPlannerResolution
{
    FPWPlannerOutcome Outcome;
    PerfectWorld::SchoolWeekPlan Plan;
    PerfectWorld::SchoolWeekResult Result;
};

/**
 * Everything the planner needs from the world (the pattern of PWSchool::IScGateWorld). The integrator's implementation runs over
 * UPWLaneLifeSubsystem and the reducer (see Docs/Phase2/Handoffs/A6.md); tests install a fake.
 */
class IPWSchoolPlannerWorld
{
public:
    virtual ~IPWSchoolPlannerWorld() = default;
    /** Pure read; costs nothing. False with a reason when there is no world. */
    virtual bool ReadPreview(FPWPlannerPreview& OutPreview, FString& OutWhy) = 0;
    /** school.week.order. Moves no clock. */
    virtual FPWPlannerOutcome SetOrder(const PerfectWorld::SchoolStandingOrder& Order, bool bRemove) = 0;
    /** school.week.resolve: the only call that can spend time. */
    virtual FPWPlannerResolution Resolve() = 0;
};

/** Words and the session recap, linked by name from the planner, the projection terminal and the automation tests. */
namespace PWSchoolPlanner
{
/** "HH:MM". */
FText ClockText(int32 MinuteOfDay);
/** "Mon 2 Jun" from a campaign day number (GameClock::DayNumber); the day after 0 is 1. */
FText DayText(int64 CampaignDay);
/** Weekday bit n = Weekday n (Sunday 0). "Mon, Tue, Wed". */
FText WeekdaysText(uint8 Weekdays);
FText ModeText(PerfectWorld::SchoolLessonMode Mode);
FText OrderTitle(uint8 Weekdays, bool bAttendSchool, PerfectWorld::SchoolLessonMode Mode);
/** Words for a refusal code of the school.week.*, week.* and dayloop.* families. Never the code itself. */
FText RefusalText(const FString& Code);

/** The plan as the player will see it. Pure. */
FPWPlannerModel BuildModel(const FPWPlannerPreview& Preview, const FPWRecapModel& Recap, const FText& Clock);
/** The recap of an actual resolution. Pure: reads only Result and the Plan it resolved. */
FPWRecapModel BuildRecap(const PerfectWorld::SchoolWeekPlan& Plan, const PerfectWorld::SchoolWeekResult& Result);
/** Reads the bridge and builds the model; bAvailable is false when it cannot. */
FPWPlannerModel ReadModel(IPWSchoolPlannerWorld& World);

// ---- The recap, session state only. Never saved: after a reload there is none and the planner says so. ----

const FPWRecapModel& LastRecap();
void StoreRecap(const FPWRecapModel& Recap);
/** For a resolve the planner did not press itself (a phone shortcut, an acceptance step): the receipt provider's entry point. */
void StoreResolution(const PerfectWorld::SchoolWeekPlan& Plan, const PerfectWorld::SchoolWeekResult& Result);
void ClearRecap();
uint32 RecapRevision();

// ---- Words by stable text key ----

/** Words for a stable key: the installed provider first (for keys other orders own, such as a lesson's TitleTextKey), then this order's built-in English. Empty when unknown. */
FText FindText(const FString& Key);
/** FindText, or the neutral words. An unknown or malformed key never reaches the screen. */
FText TextOr(const std::string& Key, const FText& Neutral);
FText Text(const TCHAR* Key);
/** The data-driven text table's lookup, installed by the integrator; with none installed the built-in English is used. */
void SetTextProvider(TFunction<bool(const FString&, FText&)> Provider);
int32 BuiltInTextCount();
const TCHAR* BuiltInTextKey(int32 Index);
const TCHAR* BuiltInTextValue(int32 Index);

// ---- Opening from the world (the phone entry, the pause menu entry) ----

/** The host installs the handler that pauses and opens the planner; with none installed the request does nothing and returns false. */
void SetOpenHandler(TFunction<void()> Handler);
bool RequestOpen();

/** Widget tags the tests search for. */
inline const TCHAR* const PanelTag = TEXT("PWSchoolPlannerPanel");
inline const TCHAR* const OrderRowTag = TEXT("PWSchoolPlannerOrder");
inline const TCHAR* const SceneDayTag = TEXT("PWSchoolPlannerSceneDay");
inline const TCHAR* const DayTag = TEXT("PWSchoolPlannerDay");
inline const TCHAR* const EntryTag = TEXT("PWSchoolPlannerEntry");
inline const TCHAR* const RecapTag = TEXT("PWSchoolPlannerRecap");
inline const TCHAR* const RecapRowTag = TEXT("PWSchoolPlannerRecapRow");
inline const TCHAR* const NoRecapTag = TEXT("PWSchoolPlannerNoRecap");
inline const TCHAR* const UnavailableTag = TEXT("PWSchoolPlannerUnavailable");
inline const TCHAR* const ProblemTag = TEXT("PWSchoolPlannerProblem");
} // namespace PWSchoolPlanner

class SPWSchoolPlanner : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWSchoolPlanner) : _Space(FVector2D(1920.0, 1080.0)) {}
        SLATE_ARGUMENT(TSharedPtr<IPWSchoolPlannerWorld>, World)
        /** The logical size of the screen the panel may use. The pause menu passes the size it already measures. */
        SLATE_ATTRIBUTE(FVector2D, Space)
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    const FPWPlannerModel& GetModel() const { return Model; }
    /** Draft editor state. Changing it never reaches the world. */
    void ToggleDraftDay(int32 Weekday);
    void SetDraftAttend(bool bAttend);
    void SetDraftMode(PerfectWorld::SchoolLessonMode Mode);
    void LoadDraft(const FString& OrderId);
    uint8 GetDraftWeekdays() const { return DraftWeekdays; }
    /** school.week.order for the draft. Returns the core's answer (also shown in the panel). */
    FPWPlannerOutcome SaveDraft();
    FPWPlannerOutcome RemoveOrder(const FString& OrderId);
    /** school.week.resolve; the recap is stored and shown when the core accepted. */
    FPWPlannerOutcome ResolveWeek();
    /** Re-reads the bridge (a cost-free read). */
    void Refresh();
    FText GetNotice() const { return Notice; }
    int32 GetResolveCount() const { return ResolveCount; }

private:
    TSharedRef<SWidget> BuildBody();
    TSharedRef<SWidget> BuildOrders();
    TSharedRef<SWidget> BuildEditor();
    TSharedRef<SWidget> BuildSceneDays();
    TSharedRef<SWidget> BuildWeek();
    TSharedRef<SWidget> BuildRecapSection();
    FVector2D GetSpace() const;
    void ScrollBody(float Delta);
    void RequestClose();
    void Rebuild(const TWeakPtr<SWidget>& FocusTarget);

    TSharedPtr<IPWSchoolPlannerWorld> World;
    TAttribute<FVector2D> Space;
    FSimpleDelegate OnClose;
    FPWPlannerModel Model;
    FText Notice;
    FString DraftId;
    uint8 DraftWeekdays = 0;
    bool bDraftAttend = true;
    PerfectWorld::SchoolLessonMode DraftMode = PerfectWorld::SchoolLessonMode::Attend;
    int32 ResolveCount = 0;
    TSharedPtr<SBox> BodyHolder;
    TSharedPtr<SScrollBox> BodyScroll;
    TWeakPtr<SWidget> BackButton;
    TWeakPtr<SWidget> SaveButton;
    TWeakPtr<SWidget> ResolveButton;
};
