#pragma once

#include "CoreMinimal.h"
#include "SchoolLessonContent.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SScrollBox;

/** One goal chip of a foreground lesson: the runner's goal name (sent back to the bridge) and its words. */
struct FPWSchoolDayGoal
{
    FString Name;
    FText Label;
};

/** One period of today's timetable. Words only: nothing here decides whether a lesson can open. */
struct FPWSchoolDayRow
{
    /** The portable binding id ("school.lesson.science.p0_mathematics"); stable, used for selection, never shown. */
    FString BindingId;
    int32 Period = 0;
    PerfectWorld::SchoolDayRowStatus Status = PerfectWorld::SchoolDayRowStatus::Upcoming;
    FText Subject;
    FText Title;
    FText Brief;
    FText Role;
    /** For example "Model a system". */
    FText StructureLabel;
    /** For example "10:20 to 11:05, Room XI Science". */
    FText TimeLabel;
    /** For example "Opens at the bell". */
    FText StatusLabel;
    /** Why the period cannot be opened now, and what to do instead; empty when neither applies. */
    FText Reason;
    FText Recovery;
    /** Set when an open period cannot be played in one mode. */
    FText ForegroundReason;
    FText RoutineReason;
    bool bForegroundAvailable = false;
    bool bRoutineAvailable = false;
    bool bAttendAvailable = false;
    TArray<FPWSchoolDayGoal> Goals;
};

/** Everything the school-day panel shows. Built from the portable SchoolDayView; it holds no state of its own. */
struct FPWSchoolDayModel
{
    /** False when the school day could not be read at all (no world, no timetable); the panel then says so. */
    bool bAvailable = false;
    bool bSchoolDay = false;
    bool bLessonOpen = false;
    FText Clock;
    /** For example "Foreground lessons today: 1 of 2". */
    FText ForegroundLine;
    /** On a day without school: the closure's words and the next school day. */
    FText Closure;
    FText NextSchoolDay;
    /** After recent absences: what happened and how to catch up. */
    FText Absence;
    FText AbsenceRecovery;
    /** Titles and explanations of the three modes: foreground, routine, attend. */
    FText ModeTitle[3];
    FText ModeBrief[3];
    TArray<FPWSchoolDayRow> Rows;

    const FPWSchoolDayRow* FindRow(const FString& BindingId) const;
    /** The row the choice starts on: the open or active period, else the first one that has not passed, else the first. */
    const FPWSchoolDayRow* StartingRow() const;
};

/**
 * What the panel needs from the world, and nothing else: the bridge. The real implementation (the integrator's) reads the lane
 * world's SchoolSessions, DayLoop and catalogues through PerfectWorld::BuildSchoolDayView and submits school.lesson.open; the
 * automation tests install a fake. Neither decision lives in the panel.
 */
class IPWSchoolDayWorld
{
public:
    virtual ~IPWSchoolDayWorld() = default;
    /** The current timetable; false (with a reason) when the world or the school content is not available. */
    virtual bool ReadModel(FPWSchoolDayModel& OutModel, FString& OutWhy) = 0;
    /** Submits school.lesson.open for this binding, mode and goal name (empty when the mode takes no goal). Empty text on success; otherwise the refusal in words. */
    virtual FText OpenLesson(const FString& BindingId, PerfectWorld::SchoolLessonMode Mode, const FString& GoalName) = 0;
};

namespace PWSchoolDay
{
/** The authored words for a text key; neutral text (never the key) for an empty or unknown one. */
FText Words(const PerfectWorld::SchoolLessonContent& Content, const std::string& Key);
/** "Mon 2 Jun 2042, 10:20" style words from the view's campaign day and minute. */
FText ClockText(int64 CampaignDay, int32 MinuteOfDay);
/** "HH:MM" from a minute of the day. */
FText TimeText(int32 MinuteOfDay);
FText StatusText(PerfectWorld::SchoolDayRowStatus Status);
FText StructureText(PerfectWorld::SchoolStructure Structure);

/** Formats the projection. */
FPWSchoolDayModel BuildModel(const PerfectWorld::SchoolDayView& View, const PerfectWorld::SchoolLessonContent& Content);

// ---- Layout: the same density switch as the quest window ----
bool Compact(float Width, float Height);
bool TwoPane(float Width, float Height);
float PanelWidth(float Width, float Height);
float PanelMaxHeight(float Width, float Height);
float ListWidth(float Width, float Height);

/** Widget tags the tests search for (widgets carry FName(tag)). */
inline const TCHAR* const PanelTag = TEXT("PWSchoolDayPanel");
inline const TCHAR* const RowTag = TEXT("PWSchoolDayRow");
inline const TCHAR* const DetailTag = TEXT("PWSchoolDayDetail");
inline const TCHAR* const ClosureTag = TEXT("PWSchoolDayClosure");
inline const TCHAR* const AbsenceTag = TEXT("PWSchoolDayAbsence");
inline const TCHAR* const UnavailableTag = TEXT("PWSchoolDayUnavailable");
inline const TCHAR* const ModeTag = TEXT("PWSchoolDayMode");
inline const TCHAR* const GoalTag = TEXT("PWSchoolDayGoal");
} // namespace PWSchoolDay

/**
 * The school-day window: today's periods on the left (subject, time and what happened or can happen), the chosen period in full on
 * the right (what the lesson is, the seat you usually take, and at its bell the three ways to spend it: a foreground lesson with a
 * goal, routine, or attending only). Why a mode or a period is unavailable, and what to do instead, is always written next to it.
 * On a day without school it shows the closure and the next school day; after recent absences it shows the recovery note.
 * It holds no rules: the model is the portable SchoolDayView, and a button only calls IPWSchoolDayWorld::OpenLesson, whose answer
 * (the core's refusal in words, or nothing) is shown under the buttons. A success calls OnOpened; the host then closes the window
 * and shows the lesson scene.
 *
 * Layout: the quest window's density switch. Wide windows (700+ logical units across) put the list and the detail side by side;
 * narrower ones stack them. Both scroll inside the card so Close never leaves the screen. No motion is used, so reduced motion is
 * respected by construction.
 *
 * Keys while it has focus:
 *   Up, Down, D-pad, left stick   choose the previous or next period
 *   Esc, J, B, menu button, LB    close
 *   right stick                   scroll the detail
 * Enter or A presses the focused button.
 */
class SPWSchoolDay : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWSchoolDay) : _Space(FVector2D(1920.0, 1080.0)) {}
        SLATE_ARGUMENT(TSharedPtr<IPWSchoolDayWorld>, World)
        /** The logical size of the screen the panel may use. */
        SLATE_ATTRIBUTE(FVector2D, Space)
        SLATE_EVENT(FSimpleDelegate, OnClose)
        SLATE_EVENT(FSimpleDelegate, OnOpened)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

    const FPWSchoolDayModel& GetModel() const { return Model; }
    const FString& GetSelectedId() const { return SelectedId; }
    /** Chooses a period by binding id; an unknown id is ignored. Returns whether the choice changed. */
    bool SelectRow(const FString& BindingId);
    /** Chooses the goal chip of the chosen foreground period by runner goal name. */
    bool SelectGoal(const FString& GoalName);
    /** Asks the world to open the chosen period in this mode (the button's own path). Returns whether it was accepted. */
    bool RequestOpen(PerfectWorld::SchoolLessonMode Mode);
    /** Reads the model again from the world and rebuilds the body. */
    void Refresh();
    bool IsTwoPane() const { return bTwoPane; }
    /** The last refusal in words, or empty. */
    const FText& GetNotice() const { return Notice; }

private:
    TSharedRef<SWidget> BuildBody();
    TSharedRef<SWidget> BuildList();
    TSharedRef<SWidget> BuildRow(const FPWSchoolDayRow& Row);
    TSharedRef<SWidget> BuildDetail();
    TSharedRef<SWidget> BuildModes(const FPWSchoolDayRow& Row);
    FVector2D GetSpace() const;
    const FPWSchoolDayRow* GetSelectedRow() const { return Model.FindRow(SelectedId); }
    FString GetSelectedGoal(const FPWSchoolDayRow& Row) const;
    bool MoveSelection(int32 Step);
    void FocusRow(const FString& BindingId);
    void RebuildDetail();
    void ScrollDetail(float Delta);
    void RequestClose();

    TSharedPtr<IPWSchoolDayWorld> World;
    FPWSchoolDayModel Model;
    FString Why;
    TAttribute<FVector2D> Space;
    FSimpleDelegate OnClose;
    FSimpleDelegate OnOpened;
    FString SelectedId;
    /** The goal chosen per period, by runner goal name; absent means the period's first goal. */
    TMap<FString, FString> GoalByRow;
    FText Notice;
    TArray<FString> RowOrder;
    TMap<FString, TSharedPtr<SWidget>> RowButtons;
    TSharedPtr<SBox> BodyHolder;
    TSharedPtr<SBox> DetailHolder;
    TSharedPtr<SScrollBox> ListScroll;
    TSharedPtr<SScrollBox> DetailScroll;
    TWeakPtr<SWidget> CloseButton;
    bool bTwoPane = true;
};
