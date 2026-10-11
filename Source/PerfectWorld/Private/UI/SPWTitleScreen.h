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

    void Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InCreator);
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
    FReply ShowCredits();
    FReply CloseCredits();

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
