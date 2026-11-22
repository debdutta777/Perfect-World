#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SScrollBox;
namespace PerfectWorld
{
struct LaneJournalView;
}

/** One person the portable journal says the player met today. Text only; nothing here is a rule. */
struct FPWJournalPersonRow
{
    FText Name;
    /** For example "Met at 08:47". */
    FText MetAt;
};

/** One agreement the portable journal says is still remembered. */
struct FPWJournalAgreementRow
{
    FText Who;
    FText Text;
};

/** Everything the Today screen shows. It is built from the portable LaneJournalView and holds no state of its own. */
struct FPWJournalModel
{
    /** False when the lane world could not be read at all (no game world); the panel then says so instead of showing empty lists. */
    bool bAvailable = false;
    /** The world clock's own words, for example "Mon 2 Jun · 07:42". */
    FText Clock;
    TArray<FPWJournalPersonRow> People;
    TArray<FPWJournalAgreementRow> Agreements;
};

/**
 * The Today journal's text and layout rules, linked by name from the automation tests and the journal_today acceptance scenario.
 * The widget calls these same functions.
 */
namespace PWJournal
{
/** Prefix of every agreement short-text key the portable projection produces. */
inline const TCHAR* const AgreementKeyPrefix = TEXT("journal.agreement.");

/** Readable English for a ShortTextKey. An unknown, empty or malformed key gives a neutral sentence, never the key itself. */
FText AgreementText(const FString& ShortTextKey);

/** "Met at HH:MM" from an absolute game minute, through the portable GameClock calendar helper. */
FText MetAtText(int64 FirstMetMinute);

/** Formats the portable projection. Order is the projection's order (people by id, agreements by person then agreement). */
FPWJournalModel BuildModel(const PerfectWorld::LaneJournalView& View, const FText& Clock);

/** Reads the one authoritative world (UPWLaneLifeSubsystem) and projects today; bAvailable is false when there is none. */
FPWJournalModel ReadModel(const UObject* WorldContext);

/** Layout: the same density switch as the pause menu (short or narrow windows are compact). */
bool Compact(float Width, float Height);
float PanelWidth(float Width, float Height);
float PanelMaxHeight(float Width, float Height);

/** Widget tags the tests and the acceptance scenario search for (widgets carry FName(tag)). */
inline const TCHAR* const PanelTag = TEXT("PWJournalPanel");
inline const TCHAR* const PersonRowTag = TEXT("PWJournalPersonRow");
inline const TCHAR* const AgreementRowTag = TEXT("PWJournalAgreementRow");
inline const TCHAR* const EmptyPeopleTag = TEXT("PWJournalEmptyPeople");
inline const TCHAR* const EmptyAgreementsTag = TEXT("PWJournalEmptyAgreements");
inline const TCHAR* const UnavailableTag = TEXT("PWJournalUnavailable");
} // namespace PWJournal

/**
 * The "Today" screen, opened from the pause menu: the people met today and the agreements remembered, with an empty state for each.
 * It is screen aligned and scales with the viewport like the pause card. The body scrolls inside the card when it is long, so the Back
 * button never leaves the screen. It reads time that is already held (it only opens inside the pause menu) and decides nothing.
 *
 * Keys while it has focus: Esc, B and the controller menu button go back; Up, Down, Page Up, Page Down, the D-pad and the sticks scroll;
 * Enter or A presses Back. No motion is used, so reduced motion is respected by construction.
 */
class SPWJournalPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWJournalPanel) : _Space(FVector2D(1920.0, 1080.0)) {}
        SLATE_ARGUMENT(FPWJournalModel, Model)
        /** The logical size of the screen the panel may use. The pause menu passes the size it already measures. */
        SLATE_ATTRIBUTE(FVector2D, Space)
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    const FPWJournalModel& GetModel() const { return Model; }
    /** For tests: the body's scroll offset, and the largest it can be. */
    float GetScrollOffset() const;
    float GetScrollOffsetOfEnd() const;

private:
    TSharedRef<SWidget> BuildBody();
    TSharedRef<SWidget> BuildPeopleSection();
    TSharedRef<SWidget> BuildAgreementsSection();
    void ScrollBody(float Delta);
    void RequestClose();

    FPWJournalModel Model;
    TAttribute<FVector2D> Space;
    FSimpleDelegate OnClose;
    TSharedPtr<SScrollBox> BodyScroll;
};
