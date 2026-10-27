#pragma once

#include "CoreMinimal.h"
#include "SchoolWork.h"
#include "Activities.h"
#include "Widgets/SCompoundWidget.h"

class SBox;

namespace PWA5
{
/** A trusted activity/lesson dispatcher supplies these offers, including its quoted
 * duration and practice-window explanation. The widget never supplies skill or XP. */
struct FPracticeOffer
{
    std::string Id;
    FText Label;
    FText Detail;
    bool bAvailable = false;
};

/** Per-host seam, like IScGateWorld. Own with a shared pointer; implementations must
 * weakly reference any UObject. No static registry or borrowed widget delegates.
 * Read/Quote/Text are free. Apply goes through the staged world transaction, reserving
 * 90 minutes at Begin and settling WorldMinutes once at Submit/Cancel. It also enforces
 * home/reachability, awake/idle, paused-clock and unrelated-command exclusion. */
class IStudyWorld
{
public:
    virtual ~IStudyWorld() = default;
    virtual bool CanUseDesk(FText& OutReason) const = 0;
    virtual bool ReadRecord(PerfectWorld::SchoolAcademicRecordView& Out, FText& OutReason) const = 0;
    virtual PerfectWorld::SchoolWorkResult Apply(const PerfectWorld::SchoolWorkCommand& Command) = 0;
    virtual PerfectWorld::AssessmentQuote Quote(std::string_view WorkId,
        const PerfectWorld::AssessmentAnswer& Answer) const = 0;
    virtual FText Text(std::string_view Key, const FText& Fallback) const = 0;
    /** Case's public name/prompt only; never return Trap or derived marking secrets. */
    virtual FText GroupPrompt(std::string_view WorkId, std::size_t Group) const = 0;
    virtual TArray<FPracticeOffer> PracticeOffers() const = 0;
    virtual PerfectWorld::SchoolIssue StartPractice(std::string_view OfferId) = 0;
};

/** Called by the home desk's existing interaction host; opens no paper or activity. */
bool InteractStudyDesk(const TSharedPtr<IStudyWorld>& World, const FSimpleDelegate& Open, FText& OutReason);
/** Uses the accepted activity's name and duration, with a core/bridge-supplied quote. */
FPracticeOffer StudyOffer(const PerfectWorld::ActivityDef& Activity, const FText& Quote, bool bAvailable);
FText Deadline(PerfectWorld::GameInstant At);
FText IssueText(const PerfectWorld::SchoolIssue& Issue);
FText Diagnostics(const PerfectWorld::AssessmentGroupMark& Mark);
}

DECLARE_DELEGATE_OneParam(FA5OpenPaper, const std::string&);

/** Reading this desk is free. Back never cancels an active paper. The host keeps the
 * same pause ownership when swapping desk/assessment panels. No decorative motion. */
class SPWStudyDesk : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWStudyDesk) : _Space(FVector2D(1920, 1080)) {}
        SLATE_ARGUMENT(TSharedPtr<PWA5::IStudyWorld>, World)
        SLATE_ATTRIBUTE(FVector2D, Space)
        SLATE_EVENT(FA5OpenPaper, OnOpenPaper)
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    void Refresh();
    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    TSharedRef<SWidget> BuildBody();
    TSharedPtr<PWA5::IStudyWorld> World;
    TAttribute<FVector2D> Space;
    FA5OpenPaper OnOpenPaper;
    FSimpleDelegate OnClose;
    TSharedPtr<SBox> Body;
    TSharedPtr<SWidget> BackButton;
    TMap<FString, TSharedPtr<SWidget>> Controls;
    FString FocusKey;
    FText Notice;
};
