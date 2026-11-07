#pragma once

#include "CoreMinimal.h"
#include "AptitudeProjection.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SScrollBox;

/**
 * Order A6: the school terminal's aptitude projection. It shows, for each of the ten paths, how much has been SEEN and how strong the
 * evidence is, and above all what has NOT been observed. Unobserved is UNKNOWN, never weak: the word, the colour and the order all say
 * so (an unobserved facet and an InsufficientObservation path read "Unknown", in the quiet text colour, never the warning colours), and
 * the window repeats that nothing here limits what the player may try. It holds no rules and no score: every band, coverage and facet
 * state is the portable AptitudeProject over the ledger the bridge supplies (R9's Awakening.ProjectionEvidence); the only figure it
 * prints is a count of recorded observations.
 *
 * Keys: Up, Down, D-pad and the left stick choose the previous or next path; Esc, B, the menu button and the left shoulder go back;
 * the right stick scrolls the detail. Wide windows put the list and the detail side by side; narrower ones stack them.
 */

struct FPWProjectionFacetRow
{
    FText Name;
    FText State;
    PerfectWorld::AptitudeFacetState StateValue = PerfectWorld::AptitudeFacetState::Unobserved;
};

struct FPWProjectionPathRow
{
    /** The portable path id ("path.scholar"); stable, never shown. */
    FString Id;
    PerfectWorld::AptitudePath Path = PerfectWorld::AptitudePath::Hunter;
    FText Name;
    /** "Unknown so far" for InsufficientObservation: unobserved is unknown, never weak. */
    FText Band;
    PerfectWorld::AptitudeBand BandValue = PerfectWorld::AptitudeBand::InsufficientObservation;
    bool bUnknown = true;
    FText Coverage;
    /** The core's own printout line for the path (words only, no digits). */
    FText Printout;
    TArray<FPWProjectionFacetRow> Facets;
    /** The facets with no evidence yet, named. */
    TArray<FText> Unknown;
    /** Recorded observations that bear on the path's facets (a count, not a score). */
    int32 ObservationCount = 0;
};

struct FPWProjectionModel
{
    /** False when the ledger could not be read or was inconsistent. */
    bool bAvailable = false;
    /** True when the ledger holds nothing at all: every path is unknown. */
    bool bNothingSeen = false;
    /** The ten paths in the core's order. */
    TArray<FPWProjectionPathRow> Paths;

    const FPWProjectionPathRow* FindRow(const FString& Id) const;
};

/** What the terminal reads: R9's projection evidence. The integrator's implementation runs over the lane-life world; tests install a fake. */
class IPWSchoolProjectionWorld
{
public:
    virtual ~IPWSchoolProjectionWorld() = default;
    /** Pure read; costs nothing. False with a reason when there is no world. */
    virtual bool ReadLedger(PerfectWorld::AptitudeLedger& OutLedger, FString& OutWhy) = 0;
};

namespace PWSchoolProjection
{
FText PathName(PerfectWorld::AptitudePath Path);
FText FacetName(PerfectWorld::AptitudeFacet Facet);
/** InsufficientObservation reads "Unknown so far". */
FText BandText(PerfectWorld::AptitudeBand Band);
FText CoverageText(PerfectWorld::AptitudeCoverage Coverage);
/** Unobserved reads "Unknown". */
FText FacetStateText(PerfectWorld::AptitudeFacetState State);

/** Pure: AptitudeProject over the ledger, formatted. */
FPWProjectionModel BuildModel(const PerfectWorld::AptitudeLedger& Ledger);
FPWProjectionModel ReadModel(IPWSchoolProjectionWorld& World);

// ---- Opening from the world (the school terminal's interaction) ----

/** The host installs the handler that pauses and opens the terminal window; with none installed the request does nothing and returns false. */
void SetOpenHandler(TFunction<void()> Handler);
bool RequestOpen();

// ---- Layout: the same switches as the quest window ----

bool TwoPane(float Width, float Height);

inline const TCHAR* const PanelTag = TEXT("PWSchoolProjectionPanel");
inline const TCHAR* const RowTag = TEXT("PWSchoolProjectionRow");
inline const TCHAR* const DetailTag = TEXT("PWSchoolProjectionDetail");
inline const TCHAR* const FacetTag = TEXT("PWSchoolProjectionFacet");
inline const TCHAR* const UnknownTag = TEXT("PWSchoolProjectionUnknown");
inline const TCHAR* const NothingSeenTag = TEXT("PWSchoolProjectionNothingSeen");
inline const TCHAR* const UnavailableTag = TEXT("PWSchoolProjectionUnavailable");
} // namespace PWSchoolProjection

class SPWSchoolProjection : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWSchoolProjection) : _Space(FVector2D(1920.0, 1080.0)) {}
        SLATE_ARGUMENT(FPWProjectionModel, Model)
        /** The logical size of the screen the panel may use. */
        SLATE_ATTRIBUTE(FVector2D, Space)
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
    virtual void OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) override;

    const FPWProjectionModel& GetModel() const { return Model; }
    const FString& GetSelectedId() const { return SelectedId; }
    /** Chooses a path by id; an unknown id is ignored. Returns whether the choice changed. */
    bool SelectRow(const FString& Id);
    /** Re-checks the arrangement against the space now; returns true when it changed. */
    bool RefreshLayout();
    bool IsTwoPane() const { return bTwoPane; }

private:
    TSharedRef<SWidget> BuildBody();
    TSharedRef<SWidget> BuildList();
    TSharedRef<SWidget> BuildDetail();
    TSharedRef<SWidget> BuildDetailContent();
    FVector2D GetSpace() const;
    const FPWProjectionPathRow* GetSelectedRow() const { return Model.FindRow(SelectedId); }
    bool MoveSelection(int32 Step);
    void FocusRow(const FString& Id);
    void RequestClose();

    FPWProjectionModel Model;
    TAttribute<FVector2D> Space;
    FSimpleDelegate OnClose;
    FString SelectedId;
    TArray<FString> RowOrder;
    TMap<FString, TSharedPtr<SWidget>> RowButtons;
    TSharedPtr<SBox> BodyHolder;
    TSharedPtr<SScrollBox> ListScroll;
    TSharedPtr<SScrollBox> DetailScroll;
    TSharedPtr<SBox> DetailHolder;
    TWeakPtr<SWidget> BackButton;
    bool bTwoPane = true;
};
