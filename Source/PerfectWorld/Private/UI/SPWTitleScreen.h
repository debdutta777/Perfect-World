#pragma once

#include "CoreMinimal.h"
#include "UI/PWDelegates.h"
#include "Widgets/SCompoundWidget.h"
#include "Animation/CurveSequence.h"

class SWidgetSwitcher;
class UPerfectWorldCharacterCreatorSubsystem;

class SPWTitleScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWTitleScreen) {}
        SLATE_EVENT(FSimpleDelegate, OnNewGame)
        SLATE_EVENT(FPWTryDelegate, OnContinue)
        SLATE_EVENT(FSimpleDelegate, OnLoad)
        SLATE_EVENT(FSimpleDelegate, OnSettings)
        SLATE_EVENT(FSimpleDelegate, OnQuit)
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
    };
    static FLayout ComputeLayout(float Width, float Height);

    void Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InCreator);
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
    FReply ShowCredits();
    FReply CloseCredits();
    FLayout CurrentLayout() const;

    FPWTryDelegate OnContinue;
    TSharedPtr<SWidgetSwitcher> ViewSwitcher;
    TWeakPtr<SWidget> MainFocusTarget;
    TWeakPtr<SWidget> CreditsFocusTarget;
    int32 ContinueSlot = -1;
    FText ContinueNote;
    FText Status;
    bool bShowingCredits = false;
    FCurveSequence PageTransition;
    FCurveHandle PageReveal;
};
