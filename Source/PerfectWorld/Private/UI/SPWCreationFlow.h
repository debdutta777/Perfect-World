#pragma once

#include "CoreMinimal.h"
#include "UI/PWDelegates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FPWCreationModel;
class SWidgetSwitcher;
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

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    /** Moves keyboard focus to the first control on the page now showing. */
    void FocusActivePage();

private:
    void GoToStep(int32 Step);
    void GoBack();
    void RequestStartLife();

    TSharedPtr<FPWCreationModel> Model;
    TSharedPtr<SWidgetSwitcher> Pages;
    FPWTryDelegate OnStartLife;
    FSimpleDelegate OnBackToTitle;
    int32 CurrentStep = 0;
};
