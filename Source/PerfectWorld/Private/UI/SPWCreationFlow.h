#pragma once

#include "CoreMinimal.h"
#include "UI/PWDelegates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Animation/CurveSequence.h"

class FPWCreationModel;
class SWidgetSwitcher;
class SScrollBox;
class UPerfectWorldCharacterCreatorSubsystem;

/**
 * The character-creation flow: a step indicator, the five pages, and Back/Next
 * (Start Life on the last page). It owns the model the pages read and holds no
 * rules itself; Start Life is enabled only when the core says it is ready.
 */
class SPWCreationFlow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWCreationFlow)
        : _InitialStep(0)
    {
    }
        /** The step to open on, as PWUI::ECreationStep. */
        SLATE_ARGUMENT(int32, InitialStep)
        /** Asked to begin life; returns empty text on success or the message to show. */
        SLATE_EVENT(FPWTryDelegate, OnStartLife)
        /** Asked to leave the creator from the first page. */
        SLATE_EVENT(FSimpleDelegate, OnBackToTitle)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InSubsystem);
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    /** Moves keyboard focus to the first control on the page now showing. */
    void FocusActivePage();

private:
    void GoToStep(int32 Step);
    void GoBack();
    void RequestStartLife();

    /** Start Life cannot be undone, so it asks once more, with Cancel focused (a stray or held accept cancels). */
    void BeginStartConfirmation();
    void CancelStartConfirmation();

    TSharedPtr<FPWCreationModel> Model;
    TSharedPtr<SWidgetSwitcher> Pages;
    TSharedPtr<SScrollBox> PageBody;
    FCurveSequence PageTransition;
    FCurveHandle PageReveal;
    TSharedPtr<SWidget> CancelStartButton;
    FPWTryDelegate OnStartLife;
    FSimpleDelegate OnBackToTitle;
    int32 CurrentStep = 0;
    bool bConfirmingStart = false;
};
