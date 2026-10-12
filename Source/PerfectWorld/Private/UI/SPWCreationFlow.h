#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UI/PWDelegates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Animation/CurveSequence.h"

class FPWCreationModel;
class SWidgetSwitcher;
class SScrollBox;
class SBox;
class SPWTraitsPage;
class SPWHouseholdPage;
class SPWStreamPage;
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
        SLATE_ATTRIBUTE(bool, NarrowLayout)
        /** Asked to begin life; returns empty text on success or the message to show. */
        SLATE_EVENT(FPWTryDelegate, OnStartLife)
        /** Asked to leave the creator from the first page. */
        SLATE_EVENT(FSimpleDelegate, OnBackToTitle)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InSubsystem);
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;
    virtual FReply OnKeyUp(const FGeometry&, const FKeyEvent&) override;
    virtual FReply OnAnalogValueChanged(const FGeometry&, const FAnalogInputEvent&) override;
    virtual FNavigationReply OnNavigation(const FGeometry&, const FNavigationEvent&) override;

    /** Moves keyboard focus to the first control on the page now showing. */
    void FocusActivePage();

private:
    void GoToStep(int32 Step);
    void GoBack();
    void OpenAllocationModal(TSharedRef<SWidget> Content, TSharedPtr<SWidget> SafeAction);
    void CloseAllocationModal();
    void TraverseModal(bool bReverse);
    bool ScrollModal(float Amount);
    void EditResources();
    void FocusDetails(TSharedPtr<SWidget> Target);
    void RequestStartLife();

    /** Start Life cannot be undone, so it asks once more, with Cancel focused (a stray or held accept cancels). */
    void BeginStartConfirmation();
    void CancelStartConfirmation();

    TSharedPtr<FPWCreationModel> Model;
    TSharedPtr<SWidgetSwitcher> Pages;
    TSharedPtr<SScrollBox> PageBody;
    TSharedPtr<SPWTraitsPage> TraitsPage;
    TSharedPtr<SPWHouseholdPage> HouseholdPage;
    TSharedPtr<SPWStreamPage> StreamPage;
    TSharedPtr<SBox> AllocationModal;
    TSharedPtr<SScrollBox> ModalScroll;
    TSharedPtr<SWidget> ModalInvoker;
    TSharedPtr<SWidget> FooterControls;
    FKey ConsumedRelease;
    bool bAllocationModal = false;
    FCurveSequence PageTransition;
    FCurveHandle PageReveal;
    FCurveSequence ModalTransition;
    FCurveHandle ModalReveal;
    TSharedPtr<SWidget> CancelStartButton;
    FPWTryDelegate OnStartLife;
    FSimpleDelegate OnBackToTitle;
    int32 CurrentStep = 0;
    bool bConfirmingStart = false;
    TAttribute<bool> NarrowLayout;
};
