#pragma once

#include "CoreMinimal.h"
#include "Framework/SlateDelegates.h"
#include "UI/PWDelegates.h"
#include "UI/TitleR5Model.h"
#include "Widgets/SCompoundWidget.h"
#include "Animation/CurveSequence.h"

class SWidgetSwitcher;
class UPerfectWorldCharacterCreatorSubsystem;

namespace PWUI
{
enum class EButtonKind : uint8;
}

class SPWTitleScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWTitleScreen) : _Saves(nullptr) {}
        SLATE_EVENT(FSimpleDelegate, OnNewGame)
        SLATE_EVENT(FPWTryDelegate, OnContinue)
        /** Opens the separate Load screen. Used when OnLoadSlot is not bound. */
        SLATE_EVENT(FSimpleDelegate, OnLoad)
        /** When bound, Load lists the saved lives on the title itself and loads the chosen slot (empty text on success). */
        SLATE_EVENT(FPWSlotDelegate, OnLoadSlot)
        SLATE_EVENT(FSimpleDelegate, OnSettings)
        SLATE_EVENT(FSimpleDelegate, OnQuit)
        /** Test seam: the saves to show instead of reading the slots from disk. Copied; may be freed afterwards. */
        SLATE_ARGUMENT(const PWTitleR5::FTitleSaves*, Saves)
    SLATE_END_ARGS()

    /**
     * Where everything goes at one logical window size (units are Slate units, after UI scaling). The menu sits in a
     * readable column on the left over a dark surface; the Fivefold Aperture takes the right third. Ornament is
     * suppressed before any control is compressed: the corner brackets and the aperture go first, then the menu column
     * narrows, and only then does the page scroll.
     */
    struct FLayout
    {
        float LeftPad = 64.0f;
        float MenuWidth = 560.0f;
        /** Where the opaque reading surface ends and its fade begins. */
        float SurfaceEdge = 664.0f;
        bool bAperture = false;
        /** The width of the box the aperture is centred in (the right third, less its margin). */
        float ApertureSlot = 0.0f;
        /** Diameter of the ring itself, 340 to 460 where space permits. */
        float ApertureDiameter = 0.0f;
        /** Drifting motes need the wider 1.5x envelope, so they appear only when the slot has that room. */
        bool bMotes = false;
        bool bBrackets = false;
        /** Short windows join the save summary's lines in pairs; the page scrolls if even that is too tall. */
        bool bCompactSummary = false;
    };
    static FLayout ComputeLayout(float Width, float Height);

    /** Overrides the logical window size the layout uses (for tests); zero or less goes back to the real geometry. */
    void SetSpaceForTest(float Width, float Height);

    void Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InCreator);
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    /** The saves this screen is showing, newest first. */
    const PWTitleR5::FTitleSaves& GetSaves() const { return Saves; }
    /** 0 the menu, 1 credits, 2 the saved lives. */
    int32 GetActiveView() const { return ActiveView; }
    /** The widget whose first focusable child takes focus when the menu opens; null once it is gone. */
    TSharedPtr<SWidget> GetMainFocusTarget() const { return MainFocusTarget.Pin(); }

private:
    enum EView : int32 { ViewMain = 0, ViewCredits = 1, ViewSaves = 2 };

    FReply ShowCredits();
    FReply ShowSaves();
    FReply ShowMain();
    FReply SwitchView(int32 View, const TWeakPtr<SWidget>& Focus);
    FLayout CurrentLayout() const;
    TSharedRef<SWidget> MakeRow(const FText& Label, PWUI::EButtonKind Kind, FOnClicked Click, bool bEnabled = true);
    TSharedRef<SWidget> BuildMainView();
    TSharedRef<SWidget> BuildCreditsView();
    TSharedRef<SWidget> BuildSavesView();
    TSharedRef<SWidget> BuildSaveCard(const PWTitleR5::FSlotSummary& Summary);
    TAttribute<FMargin> PagePadding(float Top);

    FPWTryDelegate OnContinue;
    FSimpleDelegate OnNewGame;
    FSimpleDelegate OnLoad;
    FPWSlotDelegate OnLoadSlot;
    FSimpleDelegate OnSettings;
    FSimpleDelegate OnQuit;
    PWTitleR5::FTitleSaves Saves;
    TSharedPtr<SWidgetSwitcher> ViewSwitcher;
    TWeakPtr<SWidget> MainFocusTarget;
    TWeakPtr<SWidget> CreditsFocusTarget;
    TWeakPtr<SWidget> SavesFocusTarget;
    FText Status;
    FText LoadStatus;
    int32 ActiveView = ViewMain;
    FVector2D SpaceForTest = FVector2D::ZeroVector;
    FCurveSequence PageTransition;
    FCurveHandle PageReveal;
};
